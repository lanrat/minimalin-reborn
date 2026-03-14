// DEBUG is set in build_config.js (generated at build time)

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

var Weather = function (pebble) {
  var BASE_URL = 'https://api.open-meteo.com/v1/forecast';
  var LOCATION_OPTS = {
    'timeout': 5000,
    'maximumAge': 30 * 60 * 1000
  };

  var WeatherVariable = {
    CURRENT_TEMPERATURE: 0,
    APPARENT_TEMPERATURE: 1,
    SUNSET: 2
  };

  var getWeatherVariable = function () {
    var config = Config('config').load();
    if (typeof config.weather_variable !== 'number') {
      return WeatherVariable.CURRENT_TEMPERATURE;
    }
    if (config.weather_variable < WeatherVariable.CURRENT_TEMPERATURE || config.weather_variable > WeatherVariable.SUNSET) {
      return WeatherVariable.CURRENT_TEMPERATURE;
    }
    return config.weather_variable;
  };

  // Map WMO weather code + is_day to icon charCode for custom font
  var wmoToIcon = function(code, isDay) {
    var char;
    if (code <= 1) char = 'a';                    // Clear / mainly clear
    else if (code === 2) char = 'b';              // Partly cloudy
    else if (code === 3) char = 'd';              // Overcast
    else if (code >= 45 && code <= 48) char = 'i'; // Fog
    else if (code >= 51 && code <= 57) char = 'e'; // Drizzle
    else if (code >= 61 && code <= 67) char = 'f'; // Rain
    else if (code >= 71 && code <= 77) char = 'h'; // Snow
    else if (code >= 80 && code <= 82) char = 'e'; // Rain showers
    else if (code >= 85 && code <= 86) char = 'h'; // Snow showers
    else if (code >= 95) char = 'g';               // Thunderstorm
    else char = 'b';                               // Fallback
    if (!isDay) char = char.toUpperCase();
    return char.charCodeAt(0);
  };

  var buildUrl = function (latitude, longitude, weatherVariable) {
    var url = BASE_URL + '?latitude=' + latitude + '&longitude=' + longitude + '&forecast_days=1&timezone=auto';
    var currentVars = ['weather_code', 'is_day'];
    if (weatherVariable === WeatherVariable.SUNSET) {
      url += '&daily=sunset';
    } else if (weatherVariable === WeatherVariable.APPARENT_TEMPERATURE) {
      currentVars.unshift('apparent_temperature');
    } else {
      currentVars.unshift('temperature_2m');
    }
    url += '&current=' + currentVars.join(',');
    return url;
  };

  var parseSunsetMinutes = function (sunsetIso) {
    var match = /T(\d{2}):(\d{2})/.exec(sunsetIso || '');
    if (!match) return -1;
    return parseInt(match[1], 10) * 60 + parseInt(match[2], 10);
  };

  var fetchWeather = function (latitude, longitude) {
    var req = new XMLHttpRequest();
    var weatherVariable = getWeatherVariable();
    var url = buildUrl(latitude, longitude, weatherVariable);
    debug('fetchWeather requesting:', url);
    req.open('GET', url, true);
    req.onload = function () {
      debug('fetchWeather onload, readyState:', req.readyState, 'status:', req.status);
      if (req.readyState === 4) {
        if (req.status === 200) {
          var response = JSON.parse(req.responseText);
          var icon = wmoToIcon(response.current.weather_code, response.current.is_day);
          var valuePrimary = 0;
          if (weatherVariable === WeatherVariable.APPARENT_TEMPERATURE) {
            valuePrimary = Math.round(response.current.apparent_temperature);
          } else if (weatherVariable === WeatherVariable.SUNSET) {
            valuePrimary = parseSunsetMinutes(response.daily.sunset[0]);
          } else {
            valuePrimary = Math.round(response.current.temperature_2m);
          }
          var data = {
            'AppKeyWeatherIcon': icon,
            'AppKeyWeatherTemperature': valuePrimary
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
  };

  var locationSuccess = function(pos) {
    debug('locationSuccess:', pos.coords.latitude, pos.coords.longitude);
    fetchWeather(pos.coords.latitude, pos.coords.longitude);
  };

  var locationError = function(err) {
    debug('locationError:', err.code, err.message);
    pebble.sendAppMessage({
      'AppKeyWeatherFailed': 0
    });
  };

  pebble.addEventListener('appmessage', function (e) {
    var dict = e.payload;
    debug('appmessage:', dict);
    if(dict['AppKeyWeatherRequest']) {
      debug('Weather request received, requesting geolocation...');
      window.navigator.geolocation.getCurrentPosition(locationSuccess, locationError, LOCATION_OPTS);
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
      weather_variable: 'AppKeyWeatherVariable',
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
