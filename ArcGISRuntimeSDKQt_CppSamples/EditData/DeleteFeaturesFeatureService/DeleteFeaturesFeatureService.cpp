// [WriteFile Name=DeleteFeaturesFeatureService, Category=EditData]
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

#include "DeleteFeaturesFeatureService.h"

#include "Map.h"
#include "MapQuickView.h"
#include "Basemap.h"
#include "Viewpoint.h"
#include "Point.h"
#include "SpatialReference.h"
#include "ServiceFeatureTable.h"
#include "FeatureLayer.h"
#include "Feature.h"
#include "ArcGISFeature.h"
#include "FeatureEditResult.h"
#include "FeatureQueryResult.h"
#include <QUrl>
#include <QUuid>
#include <QMouseEvent>

using namespace Esri::ArcGISRuntime;

DeleteFeaturesFeatureService::DeleteFeaturesFeatureService(QQuickItem* parent) :
  QQuickItem(parent)
{
}

DeleteFeaturesFeatureService::~DeleteFeaturesFeatureService()
{
}

void DeleteFeaturesFeatureService::init()
{
  qmlRegisterType<MapQuickView>("Esri.Samples", 1, 0, "MapView");
  qmlRegisterType<DeleteFeaturesFeatureService>("Esri.Samples", 1, 0, "DeleteFeaturesFeatureServiceSample");
}

void DeleteFeaturesFeatureService::componentComplete()
{
  QQuickItem::componentComplete();

  // find QML MapView component
  m_mapView = findChild<MapQuickView*>("mapView");
  m_mapView->setWrapAroundMode(WrapAroundMode::Disabled);

  // create a Map by passing in the Basemap
  m_map = new Map(Basemap::streets(this), this);
  m_map->setInitialViewpoint(Viewpoint(Point(-10800000, 4500000, SpatialReference(102100)), 3e7));

  // set map on the map view
  m_mapView->setMap(m_map);

  // create the ServiceFeatureTable
  m_featureTable = new ServiceFeatureTable(QUrl("http://sampleserver6.arcgisonline.com/arcgis/rest/services/DamageAssessment/FeatureServer/0"), this);

  // create the FeatureLayer with the ServiceFeatureTable and add it to the Map
  m_featureLayer = new FeatureLayer(m_featureTable, this);
  m_featureLayer->setSelectionWidth(3);
  m_map->operationalLayers()->append(m_featureLayer);

  connectSignals();
}

void DeleteFeaturesFeatureService::connectSignals()
{   
  // connect to the mouse clicked signal on the MapQuickView
  connect(m_mapView, &MapQuickView::mouseClicked, this, [this](QMouseEvent& mouseEvent)
  {
    // first clear the selection
    m_featureLayer->clearSelection();

    // set the properties for qml
    m_screenX = mouseEvent.x();
    emit screenXChanged();
    m_screenY = mouseEvent.y();
    emit screenYChanged();
    emit hideWindow();

    //! [DeleteFeaturesFeatureService identify feature]
    // call identify on the map view
    constexpr double tolerance = 5.0;
    constexpr int maxResults = 1;
    const double screenX = mouseEvent.x();
    const double screenY = mouseEvent.y();
    constexpr bool returnPopupsOnly = false;
    m_mapView->identifyLayer(m_featureLayer, screenX, screenY, tolerance, returnPopupsOnly, maxResults);
    //! [DeleteFeaturesFeatureService identify feature]
  });

  // connect to the viewpoint changed signal on the MapQuickView
  connect(m_mapView, &MapQuickView::viewpointChanged, this, [this]()
  {
    m_featureLayer->clearSelection();
    emit hideWindow();
  });

  // connect to the identifyLayerCompleted signal on the map view
  connect(m_mapView, &MapQuickView::identifyLayerCompleted, this, [this](QUuid, IdentifyLayerResult* identifyResult)
  {
    if(!identifyResult)
      return;
    if (identifyResult->geoElements().size() > 0)
    {
      // delete selected feature member if not nullptr
      if (m_selectedFeature != nullptr)
        delete m_selectedFeature;

      // select the item in the result
      QueryParameters query;
      query.setObjectIds(QList<qint64>() << identifyResult->geoElements().at(0)->attributes()->attributeValue("objectid").toLongLong());
      m_featureLayer->selectFeatures(query, SelectionMode::New);

      // set selected feature member
      m_selectedFeature = static_cast<ArcGISFeature*>(identifyResult->geoElements().at(0));
    }
  });

  // connect to the selectedFeatures signal on the feature layer
  connect(m_featureLayer, &FeatureLayer::selectFeaturesCompleted, this, [this](QUuid, FeatureQueryResult* featureQueryResult)
  {
    FeatureIterator iter = featureQueryResult->iterator();
    if (iter.hasNext())
    {
      Feature* feat = iter.next();
      // emit signal for QML
      m_featureType = feat->attributes()->attributeValue("typdamage").toString();
      emit featureTypeChanged();
      emit featureSelected();
    }
  });

  // connect to the deleteFeatureCompleted signal from the ServiceFeatureTable
  connect(m_featureTable, &ServiceFeatureTable::deleteFeatureCompleted, this, [this](QUuid, bool success)
  {
    // if delete feature was successful, call apply edits
    if (success)
      m_featureTable->applyEdits();
  });

  // connect to the applyEditsCompleted signal from the ServiceFeatureTable
  connect(m_featureTable, &ServiceFeatureTable::applyEditsCompleted, this, [this](QUuid, const QList<FeatureEditResult*>& featureEditResults)
  {
    // obtain the first item in the list
    FeatureEditResult* featureEditResult = featureEditResults.isEmpty() ? nullptr : featureEditResults.first();
    // check if there were errors, and if not, log the new object ID
    if (featureEditResult && !featureEditResult->isCompletedWithErrors())
      qDebug() << "Successfully deleted Object ID:" << featureEditResult->objectId();
    else
      qDebug() << "Apply edits error.";
  });
}

void DeleteFeaturesFeatureService::deleteSelectedFeature()
{
  m_featureTable->deleteFeature(m_selectedFeature);
}

int DeleteFeaturesFeatureService::screenX() const
{
  return m_screenX;
}

int DeleteFeaturesFeatureService::screenY() const
{
  return m_screenY;
}

QString DeleteFeaturesFeatureService::featureType() const
{
  return m_featureType;
}
