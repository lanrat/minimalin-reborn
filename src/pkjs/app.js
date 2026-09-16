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

var Weather = function(pebble){
  var BASE_URL = 'https://api.open-meteo.com/v1/forecast';
  var LOCATION_OPTS = {
    'timeout': 5000,
    'maximumAge': 30 * 60 * 1000
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

  var fetchWeather = function(latitude, longitude) {
    var req = new XMLHttpRequest();
    // timezone=auto so the daily high/low cover the local day, not a UTC one.
    var url = BASE_URL + '?latitude=' + latitude + '&longitude=' + longitude +
      '&current=temperature_2m,weather_code,is_day' +
      '&daily=temperature_2m_max,temperature_2m_min&forecast_days=1&timezone=auto';
    debug('fetchWeather requesting:', url);
    req.open('GET', url, true);
    req.onload = function () {
      debug('fetchWeather onload, readyState:', req.readyState, 'status:', req.status);
      if (req.readyState === 4) {
        if (req.status === 200) {
          var response = JSON.parse(req.responseText);
          var temperature = Math.round(response.current.temperature_2m);
          var icon = wmoToIcon(response.current.weather_code, response.current.is_day);
          var data = {
            'AppKeyWeatherIcon': icon,
            'AppKeyWeatherTemperature': temperature
          };
          var daily = response.daily;
          if (daily && daily.temperature_2m_max && daily.temperature_2m_max.length &&
              daily.temperature_2m_min && daily.temperature_2m_min.length) {
            // Sent as a pair; the watch only shows the range when it has both.
            data['AppKeyWeatherHigh'] = Math.round(daily.temperature_2m_max[0]);
            data['AppKeyWeatherLow'] = Math.round(daily.temperature_2m_min[0]);
          }
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
      refresh_rate: 'AppKeyRefreshRate',
      rainbow_mode: 'AppKeyRainbowMode',
      vibrate_on_the_hour: 'AppKeyVibrateOnTheHour',
      military_time: 'AppKeyMilitaryTime',
      quiet_time_visible: 'AppKeyQuietTimeVisible',
      extra_detail: 'AppKeyExtraDetail'
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
