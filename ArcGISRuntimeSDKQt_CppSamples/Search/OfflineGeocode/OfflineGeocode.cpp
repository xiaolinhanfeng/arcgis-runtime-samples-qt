// [WriteFile Name=OfflineGeocode, Category=Search]
// [Legal]
// Copyright 2016 Esri.

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// [Legal]

#include "OfflineGeocode.h"

#include "Map.h"
#include "CalloutData.h"
#include "GeocodeParameters.h"
#include "GeocodeResult.h"
#include "MapQuickView.h"
#include "Point.h"
#include "LocatorTask.h"
#include "GraphicsOverlay.h"
#include "SuggestListModel.h"
#include "IdentifyGraphicsOverlayResult.h"
#include "ArcGISTiledLayer.h"
#include "PictureMarkerSymbol.h"

#include <QQmlProperty>

using namespace Esri::ArcGISRuntime;

OfflineGeocode::OfflineGeocode(QQuickItem* parent):
    QQuickItem(parent),
    m_map(nullptr),
    m_mapView(nullptr),
    m_pinGraphic(nullptr),
    m_tiledLayer(nullptr),
    m_calloutData(nullptr),
    m_locatorTask(nullptr),
    m_graphicsOverlay(nullptr),
    m_suggestListModel(nullptr),
    m_isPressAndHold(false),
    m_isReverseGeocode(false),
    m_suggestInProgress(false),
    m_noResults(false),
    m_geocodeInProgress(false)
{
}

OfflineGeocode::~OfflineGeocode()
{
}

void OfflineGeocode::componentComplete()
{
    QQuickItem::componentComplete();

    // find QML MapView component
    m_mapView = findChild<MapQuickView*>("mapView");
    m_mapView->setWrapAroundMode(WrapAroundMode::Disabled);

    m_dataPath = QQmlProperty::read(this, "dataPath").toString();

    // create a basemap using a tiled layer
    //QUrl path = m_dataPath + "/tpk/streetmap_SD.tpk"; This won't work for whatever reason
    m_tiledLayer = new ArcGISTiledLayer(new TileCache("C:/Users/ryan8759/ArcGIS/Runtime/Data/tpk/streetmap_SD.tpk"), this);
    Basemap* basemap = new Basemap(this);
    basemap->baseLayers()->append(m_tiledLayer);

    // create map using basemap
    m_map = new Map(basemap);
    m_map->setInitialViewpoint(Viewpoint(Point(-13042254.715252, 3857970.236806, SpatialReference(3857)), 2e4));

    // Set map to map view
    m_mapView->setMap(m_map);

    // initialize callout
    m_mapView->calloutData()->setVisible(false);
    m_mapView->calloutData()->setTitle("Location");
    m_mapView->calloutData()->setImageUrl(QUrl("qrc:/Samples/Search/OfflineGeocode/RedShinyPin.png"));
    m_calloutData = m_mapView->calloutData();
    emit calloutDataChanged();

    // create locator task
    m_locatorTask = new LocatorTask(m_dataPath + "/Locators/SanDiego_StreetAddress.loc");

    m_suggestListModel = m_locatorTask->suggestions();
    emit suggestionsChanged();

    // set geocode parameters
    m_geocodeParameters.setMinScore(75);
    m_geocodeParameters.setResultAttributeNames(QStringList() << "Match_addr");
    m_geocodeParameters.setMaxResults(1);
    m_reverseGeocodeParameters.setMaxResults(1);


    // create graphics overlay and add pin graphic
    m_graphicsOverlay = new GraphicsOverlay(this);
    m_pinGraphic = new Graphic(Point(-13042254.715252, 3857970.236806, SpatialReference(3857)), new PictureMarkerSymbol(QUrl("qrc:/Samples/Search/OfflineGeocode/red_pin.png"), this), this);
    m_pinGraphic->setVisible(false);
    m_graphicsOverlay->graphics()->append(m_pinGraphic);
    m_mapView->graphicsOverlays()->append(m_graphicsOverlay);

    connectSignals();
}

void OfflineGeocode::geocodeWithText(const QString& address)
{
    m_locatorTask->geocodeWithParameters(address, m_geocodeParameters);
}

void OfflineGeocode::geocodeWithSuggestion(int index)
{
    m_locatorTask->geocodeWithSuggestResultAndParameters(m_suggestListModel->suggestResults().at(index), m_geocodeParameters);
}

void OfflineGeocode::setSuggestionsText(const QString& searchText)
{
    m_suggestListModel->setSearchText(searchText);
}

CalloutData *OfflineGeocode::calloutData() const
{
    return m_calloutData;
}

SuggestListModel *OfflineGeocode::suggestions() const
{
    return m_suggestListModel;
}

bool OfflineGeocode::geocodeInProgress() const
{
    return m_geocodeInProgress;
}

bool OfflineGeocode::suggestInProgress() const
{
    return m_suggestInProgress;
}

bool OfflineGeocode::noResults() const
{
    return m_noResults;
}

void OfflineGeocode::connectSignals()
{
    connect(m_mapView, &MapQuickView::mouseClick, [this](QMouseEvent& mouseEvent){
        m_clickedPoint = Point(m_mapView->screenToLocation(mouseEvent.x(), mouseEvent.y()));
        m_mapView->identifyGraphicsOverlay(m_graphicsOverlay, mouseEvent.x(), mouseEvent.y(), 5, 1);
    });

    connect(m_mapView, &MapQuickView::mousePressAndHold, [this](QMouseEvent& mouseEvent){
        m_isPressAndHold = true;
        m_isReverseGeocode = true;

        // reverse geocode
        m_locatorTask->reverseGeocodeWithParameters(Point(m_mapView->screenToLocation(mouseEvent.x(), mouseEvent.y())), m_reverseGeocodeParameters);

        // make busy indicator visible
        m_geocodeInProgress = true;
        emit geocodeInProgressChanged();
    });

    connect(m_mapView, &MapQuickView::mouseMove, [this](QMouseEvent& mouseEvent){
        // if user is dragging mouse hold, realtime reverse geocode
        if(m_isPressAndHold)
        {
            m_locatorTask->reverseGeocodeWithParameters(Point(m_mapView->screenToLocation(mouseEvent.x(), mouseEvent.y())), m_reverseGeocodeParameters);

            m_geocodeInProgress = true;
            emit geocodeInProgressChanged();
        }
    });

    // reset after user stops holding down mouse
    connect(m_mapView, &MapQuickView::mouseRelease, [this](){
        m_isPressAndHold = false;
        m_isReverseGeocode = false;
    });

    // dismiss no results notification
    connect(m_mapView, &MapQuickView::mousePress, [this](){
        m_noResults = false;
        emit noResultsChanged();
    });

    connect(m_suggestListModel, &SuggestListModel::suggestInProgressChanged, [this]()
    {
        m_suggestInProgress = m_suggestListModel->suggestInProgress();
        emit suggestInProgressChanged();
        emit suggestionsChanged();
    });

    connect(m_mapView, &MapQuickView::identifyGraphicsOverlayCompleted, [this](QUuid, QList<Graphic*> identifyResults){
        // if user clicked on pin, display callout
        if (identifyResults.count() > 0)
            m_calloutData->setVisible(true);
        // otherwise, reverse geocode at that point
        else
        {
            m_isReverseGeocode = true;
            m_locatorTask->reverseGeocodeWithParameters(m_clickedPoint, m_reverseGeocodeParameters);

            m_geocodeInProgress = true;
            emit geocodeInProgressChanged();
        }
    });

    connect(m_locatorTask, &LocatorTask::geocodeCompleted, [this](QUuid, QList<GeocodeResult> geocodeResults)
    {
        // dismiss busy indicator
        m_geocodeInProgress = false;
        emit geocodeInProgressChanged();

        if(geocodeResults.length() > 0)
        {
            // dismiss no results notification
            m_noResults = false;
            emit noResultsChanged();

            // dismiss callouts
            m_calloutData->setVisible(false);

            // zoom to result's extent
            m_mapView->setViewpointGeometry(geocodeResults.at(0).extent());

            // set callout location and detail
            m_calloutData->setLocation(geocodeResults.at(0).displayLocation());
            m_calloutData->setDetail(geocodeResults.at(0).label());

            // set pin graphic's location
            m_pinGraphic->setGeometry(geocodeResults.at(0).displayLocation());
            m_pinGraphic->setVisible(true);

            if (m_isReverseGeocode)
                m_calloutData->setVisible(true);

            // continue reverse geocoding if user is pressing and holding
            if (!m_isPressAndHold)
                m_isReverseGeocode = false;
        }
        // if there are no matching results, notify user
        else
        {
            m_noResults = true;
            emit noResultsChanged();
        }
    });

}
