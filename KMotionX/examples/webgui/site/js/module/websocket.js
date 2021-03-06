(function() {
  var module = angular.module('WebSocketHandler', []);
  
  
  
  module.factory('SocketService', function($rootScope) {
    var socketWorker;
    var acknowledge;
     
    var init = function(){
      if(socketWorker){
        return;
      }
    /*
      function testWs() {
        logHandler('_CONNECTING...', LOG_TYPE.NONE);

        var websocket = new WebSocket('ws://' + window.location.host + '/ws');
        
        websocket.onopen = function(ev) {
            logHandler('_CONNECTED', LOG_TYPE.NONE);
            websocket.send('KMotionX');
        };
        websocket.onclose = function(ev) {
            logHandler('_DISCONNECTED', LOG_TYPE.NONE);
            setTimeout(function() {
              testWs();
            }, 5000);
        };
        websocket.onerror = function(ev) {
            logHandler(ev.data, LOG_TYPE.ERROR);
        };

        websocket.onmessage = function(ev) {

            if (!ev.data) {
                logHandler('', LOG_TYPE.PING);
            } else {
                var obj = JSON.parse(ev.data);
                
                var handler = socketHandlers[CB_Type[obj.type]];// || _this.defaultHandler;
                var ret = handler(obj);
                //TODO only ack messages that require users answer here
                //acknowledge(obj, ret);  
                
                var ack = "CB_ACK:" + obj.id + ":" + obj.type + ":" + ret + ":";
                console.log(ack);
                websocket.send(ack);
                
                logHandler(ev.data, LOG_TYPE.RECEIVE);
                //_this.messageHandler(ev.data);
            }

        }

      }
      
      testWs();
      */
      var socketWorker = new Worker("js/socket-worker.js");
      var url = 'ws://' + window.location.host + '/ws';
      socketWorker.postMessage({command:'connect',url:url}) 
      
      socketWorker.onmessage = function(event) {
        var data = event.data; 
        if(data.data){
          var obj = JSON.parse(data.message);
          var handler = socketHandlers[obj.type];// || _this.defaultHandler;
          var ret = handler(obj);
          if(obj.block){
            //only ack messages that require users answer here
            acknowledge(obj, ret);              
          }
          
        } else if(data.log){
          logHandler(data.message, data.type);
        }
      }
      acknowledge = function(obj, ret){
        socketWorker.postMessage({command:'acknowledge',obj:obj,ret:ret});
      } 
      
    }
    var statusHandler = function(obj) {
      //select gcode row
      var line = obj.data.line;
      angular.element(document.getElementById("gcc")).scope().selectLine(line);
      //gcodeText.select(line);
      //viewer.draw(obj.data.message);
      logText('status', 'green', line + ": " + obj.data.message);
    }
    var completeHandler = function(obj) {
      //select gcode row
      var line = obj.data.line;
      angular.element(document.getElementById("gcc")).scope().selectLine(line);
      //gcodeText.select(line);
      logText('status', 'green', "Done: " + line + ": " + obj.data.message);
    }
    var errorMessageHandler =function(obj) {
      logText('error', 'red', obj.data);
    }
    var consoleHandler = function(obj) {
      logText('console', 'blue', obj.data);
    }
    
    var userHandler = function(obj) {
      if (confirm('USR: ' + obj.data + ' ?')) {
        return 0; // zero is true for this callback
      } else {
          return 1;
      }
    }
    var userMCodeHandler = function(obj) {
      if (confirm('Are you sure you want to continue after M' + obj.data + ' ?')) {
        return 0; // zero is true for this callback
      } else {
          return 1;
      }
    }
    var stateHandler = function(obj) {
      $( "#feed_hold_btn" ).toggleClass( "button_pressed", obj.data.feedHold == 1 );

      //only load if different from loaded
      //TODO or if file has been updated, need a force flag
      /*
      var lastLoaded = localStorage.getItem('last-loaded');
      if (obj.data.file != "" && obj.data.file != lastLoaded) {
          loadGCodeFromPath(obj.data.file);
      }
      */
      $rootScope.$broadcast('state-update', { state: obj.data });
      
      //TODO listen for machine configuration changes
    }
    var messageHandler = function(obj) {
      alert(obj.data.message);
      return 0;
    }
    
    var socketHandlers = {
        
        STATUS: statusHandler,// 0: Non blocking callback. Called from the interpreter in different thread
        COMPLETE: completeHandler,// 1: Non blocking callback. Called from the interpreter in different thread
        ERR_MSG: errorMessageHandler,// 2: Non blocking callback
        CONSOLE: consoleHandler,// 3: Non blocking callback, event though it has return value??
        USER: userHandler,// 4: Blocking callback. Called from the interpreter in different thread
        USER_M_CODE: userMCodeHandler,// 5: Blocking callback. Called from the interpreter in different thread
        STATE: stateHandler,// 6: Non blocking callback. Updates gui with current state from server.
        MESSAGEBOX: messageHandler// 7: Blocking callback. However there is no need to block OK only boxes.
    }
        
    
    
    return {
        acknowledge: acknowledge,
        init: init
    }
  }); 

  
  function logText(elementId, color, text) {
    var fragment=document.createDocumentFragment();
    var div = document.createElement('div');
    var sp = document.createElement('sp');
    sp.style.color = color;
    sp.appendChild(document.createTextNode(text));
    div.appendChild(sp);
    fragment.appendChild(div);
    var logNode = document.getElementById(elementId);
    logNode.appendChild(fragment);
    logNode.scrollTop = logNode.scrollHeight;
}
  
var LOG_TYPE = {
    'NONE':0,
    'SEND':1,
    'RECEIVE':2,
    'ERROR':3,
    'PING':4
  }  





var logHandler = function(message, type) {
  this.logNode = document.getElementById('output');
  
  if (this.logNode) {
      var style = '';
      var fragment=document.createDocumentFragment();
      var div = document.createElement('div');
      fragment.appendChild(div);
      if (type == LOG_TYPE.NONE) {

      } else {
        var sp = document.createElement('span');
        if (type == LOG_TYPE.SEND) {
          sp.style.color = 'green';
          sp.appendChild(document.createTextNode("SENT: "));
        } else if (type == LOG_TYPE.RECEIVE) {
          sp.style.color = 'blue';
          sp.appendChild(document.createTextNode("RECEIVED: "));
        } else if (type == LOG_TYPE.ERROR) {
          sp.style.color = 'red';
          sp.appendChild(document.createTextNode("ERROR: "));
        } else if (type == LOG_TYPE.PING) {
          sp.style.color = 'blue';          
          sp.appendChild(document.createTextNode("PING..."));          
        }
        div.appendChild(sp);
      } 
      div.appendChild(document.createTextNode(message));

      this.logNode.appendChild(fragment);
      this.logNode.scrollTop = this.logNode.scrollHeight;
  } else {
      if (type == LOG_TYPE.NONE) {
          console.info('---',message);
      } else if (type == LOG_TYPE.SEND) {
          console.info('SENT',message);
      } else if (type == LOG_TYPE.RECEIVE) {
          console.info('RECEIVED',message);
      } else if (type == LOG_TYPE.ERROR) {
          console.info('ERROR',message);
      } else if (type == LOG_TYPE.PING) {
          console.info('PING...',message);
      }

  }
}
  
  
})();