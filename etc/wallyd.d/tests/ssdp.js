var p = nucleus.dofile('deps/utils.js').prettyPrint;
var uv = nucleus.uv;

var discovery =true;

var searchST = 'freshx:wally';
var broadcastPort = 1900;
var broadcastAddress = '239.255.255.250';
var broadcastString = 'M-SEARCH * HTTP/1.1\r\nHOST:'+broadcastAddress+':'+broadcastPort+'\r\nMAN:\'ssdp:discover\'\r\nST:ssdp:all\r\nNT:'+searchST+'\r\nMX:1\r\n\r\n';

var server = new uv.Udp();
server.bind('0.0.0.0',8080);
server.broadcast(true);

server.send(broadcastAddress, broadcastPort, broadcastString , function (err) {
  if (err) {throw err;}
  p('SSDP Packet sent. Waiting for response', server);
  server.readStart(function (err, chunk) {
    if (err) {throw err;}
    var header = {};
    var temp = chunk.toString().split(/\r\n/);
    for(var i in temp){
	var kv = temp[i].split(/:/);
	if(kv.length>1){
	    header[kv[0]] = temp[i].replace(/^.*?:/,'').trim();
	}
    }
//    p('Server onread', header);
    if(header.ST === searchST && header.LOCATION){
	  location = header.LOCATION;
	  print('Found location : '+location+', stopping SSDP discovery');
	  discovery = false;
	  server.readStop();
    }
  });
});

p(server);

var timer1 = new uv.Timer();
timer1.start(1000, 2000, function () {
  if (discovery !== true) {
       timer1.stop();
       timer1.close();
       return;
  }
  server.send(broadcastAddress, broadcastPort, broadcastString , function (err) {
    if(err) {throw err;}
    print('Sent ssdp package');
  });
});

uv.run();
