import QtQuick 2.9
import QtLocation 5.9
import QtPositioning 5.9

Item {
  id: app
  width: 900
  height: 1020

  property bool initialized: false
  property variant gpsLocation
  property variant carPosition: QtPositioning.coordinate()
  property real carBearing: 0
  property bool satelliteMode: false
  property bool mapFollowsCar: true
  property bool lockedToNorth: false

  // animation durations for continuously updated values shouldn't be much greater than updateInterval
  // ...otherwise, animations are always trying to catch up as their target values change
  property real updateInterval: 50

  Plugin {
    id: mapPlugin

    name: "mapboxgl"
    PluginParameter { name: "mapboxgl.access_token"; value: mapboxAccessToken }
    PluginParameter { name: "mapboxgl.mapping.use_fbo"; value: "false" }
    PluginParameter { name: "mapboxgl.mapping.cache.directory"; value: "/tmp/tile_cache" }
    // // necessary to draw route paths underneath road labels
    // PluginParameter { name: "mapboxgl.mapping.items.insert_before"; value: "road-label-small" }
  }


  onGpsLocationChanged: {
    carPosition = QtPositioning.coordinate(gpsLocation.latitude, gpsLocation.longitude, gpsLocation.altitude)
    carBearing = gpsLocation.bearingDeg
    initialized = true
  }

  onCarPositionChanged: {
    if (mapFollowsCar) {
      map.center = carPosition
    }
  }

  onCarBearingChanged: {
    if (mapFollowsCar && !lockedToNorth) {
      map.bearing = carBearing
    }
  }

  Map {
    id: map
    plugin: mapPlugin

    // TODO is there a better way to make map text bigger?
    width: parent.width / scale
    height: parent.height / scale
    scale: 2.5
    x: width * (scale - 1)
    y: height * (scale - 1)
    transformOrigin: Item.BottomRight

    gesture.enabled: true
    center: QtPositioning.coordinate()
    bearing: 0
    zoomLevel: 15
    copyrightsVisible: false // TODO re-enable

    // keep in sync with car indicator
    // TODO commonize with car indicator
    Behavior on center {
      CoordinateAnimation {
        easing.type: Easing.Linear;
        duration: updateInterval;
      }
    }

    Behavior on bearing {
      RotationAnimation {
        direction: RotationAnimation.Shortest
        easing.type: Easing.InOutQuad
        duration: updateInterval
      }
    }

    // TODO combine with center animation
    Behavior on zoomLevel {
      SmoothedAnimation {
        velocity: 2;
      }
    }

    onSupportedMapTypesChanged: {
      for (var i in supportedMapTypes){
        let mt = supportedMapTypes[i];
        if (mt.style === MapType.CarNavigationMap && mt.night){
          activeMapType = mt;
          break;
        }
      }
    }

    onBearingChanged: {
    }

    gesture.onPanStarted: {
      mapFollowsCar = false
    }

    gesture.onPinchStarted: {
      mapFollowsCar = false
    }

    MapQuickItem {
      id: car
      visible: carPosition.isValid //&& map.zoomLevel > 10
      anchorPoint.x: icon.width / 2
      anchorPoint.y: icon.height / 2

      opacity: 0.8
      coordinate: carPosition
      rotation: carBearing - map.bearing

      Behavior on coordinate {
        CoordinateAnimation {
          easing.type: Easing.Linear;
          duration: updateInterval;
        }
      }

      sourceItem: Image {
        id: icon
        source: "arrow-night.svg"
        width: 60 / map.scale
        height: 60 / map.scale
      }
    }
  }

  Column {
    id: buttons
    anchors.left: parent.left
    anchors.bottom: parent.bottom

    MouseArea {
      id: compass
      visible: mapFollowsCar || !lockedToNorth
      width: 125
      height: 113
      onClicked: {
        lockedToNorth = !lockedToNorth
        map.bearing = lockedToNorth || !mapFollowsCar ? 0 : carBearing
      }
      Image {
        source: "compass.svg"
        rotation: map.bearing
        anchors.centerIn: parent
        anchors.verticalCenterOffset: 5
        width: 75
        height: 75

        scale: compass.pressed ? 0.85 : 1.0
        Behavior on scale { NumberAnimation { duration: 100 } }
      }
    }

    MouseArea {
      id: location
      width: 125
      height: 113
      onClicked: {
        if (carPosition.isValid) {
          mapFollowsCar = !mapFollowsCar
          if (mapFollowsCar) {
            lockedToNorth = false
            map.zoomLevel = 16
            map.center = carPosition
            map.bearing = carBearing
          }
        }
      }
      Image {
        source: mapFollowsCar && carPosition.isValid ? "location-active.png" : "location-night.png"
        opacity: mapFollowsCar && carPosition.isValid ? 0.5 : 1.0
        width: 63
        height: 63
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -5

        scale: location.pressed ? 0.85 : 1.0
        Behavior on scale { NumberAnimation { duration: 100 } }
      }
    }
  }

}
