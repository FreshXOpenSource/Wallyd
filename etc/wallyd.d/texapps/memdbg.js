'use strict';

var wally = new Wally();
var gui = new GUI();
var date = new Date();
var uv = nucleus.uv;
var timeout = 15000;

var memstartb = wally.getrss();
var memstart = Math.round((memstartb/1024/1024)*100)/100+'mb';
var start = date.getTime();
var div = 1;
var lastmin = memstartb;

function oninterval() {
    try {
	var now = new Date();
	var mymem = wally.getrss();
	div = (now.getTime()-start)/1000;
	if(Math.ceil(div) % 60 < 2){
		lastmin = wally.getrss();
	}
	var grow = Math.ceil((mymem-memstartb));
    	var stat = 'Mem Start: '+memstart+
		 ' / curr '+Math.ceil((mymem/(1024*1024))*100)/100+'mb'+
		 ' / loss '+Math.ceil((grow)/(1024*1024)*100)/100+'mb'+
		 ' / gpm '+(mymem-lastmin)+
		 ' / T '+div+'s';
    	var tstat = ' Mem : '+Math.ceil((mymem/(1024*1024))*100)/100+'mb'+
		 ' / L '+Math.ceil((grow)/(1024*1024)*100)/100+'mb'+
		 ' / gpm '+(mymem-lastmin)+
		 ' / T '+Math.ceil(div)+'s';
	var TA = new Transaction();
        TA.push(gui.clearTexture.bind(null,'memdbg'));
	TA.push(wally.setText.bind(null,'memdbg','black','logfont',0,0,tstat));
	TA.push(wally.render.bind(null,'memdbg'));
	TA.commit();
	log.info(stat);
    } catch(err) {
	log.error('Error in memdbg : '+err);
    }
}

try {
    var memtimer = new uv.Timer();
    memtimer.start( 0, timeout, oninterval);
} catch(e) {
    log.error('Error in memdbg timer : '+e);
}
