// DEBUG is set in build_config.js (generated at build time)
// API_ID is set in weather_id.js (generated at build time)

function debug() {
  if (!window.DEBUG) return;
  var args = Array.prototype.slice.call(arguments).map(function(arg) {
    return (typeof arg === 'object' && arg !== null) ? JSON.stringify(arg) : arg;
  });
  console.log.apply(console, args);
}

var Config = function(name){
  var load = function(){
    try {
      var config = localStorage.getItem(name);
      if(config !== null){
        return JSON.parse(config);
      }
    }catch(e){}
    return {};
  };
  var store = function(data){
    try{
      localStorage.setItem(name, JSON.stringify(data));
    }catch(e){}
  };
  return {
    load: load,
    store: store
  }
};

var Weather = function(pebble){
  var METHOD = 'GET';
  var BASE_URL = 'http://api.openweathermap.org/data/2.5/weather';
  var ICONS = {
    '01d': 'a',
    '02d': 'b',
    '03d': 'c',
    '04d': 'd',
    '09d': 'e',
    '10d': 'f',
    '11d': 'g',
    '13d': 'h',
    '50d': 'i',
    '01n': 'A',
    '02n': 'B',
    '03n': 'C',
    '04n': 'D',
    '09n': 'E',
    '10n': 'F',
    '11n': 'G',
    '13n': 'H',
    '50n': 'I',
  };
  var LOCATION_OPTS = {
    'timeout': 5000,
    'maximumAge': 30 * 60 * 1000
  };

  var parseIcon = function(icon){
    return ICONS[icon].charCodeAt(0);
  }

  var fetchWeatherForLocation = function(location){
    var query = 'q=' + location;
    fetchWeather(query);
  }

  var fetchWeatherForCoordinates = function(latitude, longitude){
    var query = 'lat=' + latitude + '&lon=' + longitude;
    fetchWeather(query);
  }

  var fetchWeather = function(query) {
    debug('fetchWeather called with query:', query);
    debug('API_ID defined:', typeof window.API_ID !== 'undefined', 'value length:', window.API_ID ? window.API_ID.length : 'N/A');
    if (!window.API_ID || window.API_ID.length == 0) {
      console.error("no weather API key");
      Pebble.sendAppMessage({ 'AppKeyWeatherFailed': 1 });
      return;
    }
    debug('API key check passed');
    var req = new XMLHttpRequest();
    debug('XMLHttpRequest created');
    query += '&cnt=1&appid=' + window.API_ID;
    var url = BASE_URL + '?' + query;
    debug('fetchWeather requesting:', url);
    req.open(METHOD, url, true);
    req.onload = function () {
      debug('fetchWeather onload, readyState:', req.readyState, 'status:', req.status);
      if (req.readyState === 4) {
        if (req.status === 200) {
          var response = JSON.parse(req.responseText);
          var timestamp = response.dt;
          var temperature = Math.round(response.main.temp - 273.5);
          var icon = parseIcon(response.weather[0].icon);
          var city = response.name;
          var data = {
            'AppKeyWeatherIcon': icon,
            'AppKeyWeatherTemperature': temperature
          };
          debug('fetchWeather success, sending:', data);
          Pebble.sendAppMessage(data);
        } else {
          debug('fetchWeather error, status:', req.status, 'response:', req.responseText);
          Pebble.sendAppMessage({ 'AppKeyWeatherFailed': 1 });
        }
      }
    };
    req.onerror = function() {
      debug('fetchWeather network error:', req.status, req.statusText);
      Pebble.sendAppMessage({ 'AppKeyWeatherFailed': 1 });
    };
    req.send(null);
  }

  var locationSuccess = function(pos) {
    debug('locationSuccess:', pos.coords.latitude, pos.coords.longitude);
    var coordinates = pos.coords;
    fetchWeatherForCoordinates(coordinates.latitude, coordinates.longitude);
  }

  var locationError = function(err) {
    debug('locationError:', err.code, err.message);
    pebble.sendAppMessage({
      'AppKeyWeatherFailed': 0
    });
  }

  pebble.addEventListener('appmessage', function (e) {
    var dict = e.payload;
    debug('appmessage:', dict);
    if(dict['AppKeyWeatherRequest']) {
      debug('Weather request received');
      var config = Config('config');
      var location = config.load().location;
      if(location){
        debug('Using configured location:', location);
        fetchWeatherForLocation(location);
      }else{
        debug('Requesting geolocation...');
        window.navigator.geolocation.getCurrentPosition(locationSuccess, locationError, LOCATION_OPTS);
      }
    }
  });
}(Pebble);


Pebble.addEventListener('ready', function (e) {
  debug('JS ready');
  var data = { 'AppKeyJsReady': 1 };
  Pebble.sendAppMessage(data);
});


Pebble.addEventListener('showConfiguration', function() {
  var URL = 'https://lanrat.github.io/minimalin-reborn/';
  var config = Config('config');
  var params = config.load();
  params.platform = Pebble.getActiveWatchInfo().platform;
  var query = '?config=' + encodeURIComponent(JSON.stringify(params));
  Pebble.openURL(URL + query);
});

Pebble.addEventListener('webviewclosed', function(e) {
  if(e.response){
    var config = Config('config');
    var configData = JSON.parse(decodeURIComponent(e.response));
    config.store(configData);
    var mapping = {
      minute_hand_color: 'AppKeyMinuteHandColor',
      hour_hand_color: 'AppKeyHourHandColor',
      background_color: 'AppKeyBackgroundColor',
      time_color: 'AppKeyTimeColor',
      info_color: 'AppKeyInfoColor',
      date_displayed: 'AppKeyDateDisplayed',
      health_enabled: 'AppKeyHealthEnabled',
      weather_enabled: 'AppKeyWeatherEnabled',
      bluetooth_icon: 'AppKeyBluetoothIcon',
      battery_displayed_at: 'AppKeyBatteryDisplayedAt',
      temperature_unit: 'AppKeyTemperatureUnit',
      refresh_rate: 'AppKeyRefreshRate',
      rainbow_mode: 'AppKeyRainbowMode',
      vibrate_on_the_hour: 'AppKeyVibrateOnTheHour',
      military_time: 'AppKeyMilitaryTime'
    };
    var dict = { AppKeyConfig: 1 };
    for(var key in mapping){
      if (mapping.hasOwnProperty(key)) {
        dict[mapping[key]] = configData[key];
      }
    }
    dict['AppKeyConfig'] = 1;
    Pebble.sendAppMessage(dict, function() {}, function() {});
  }
});
