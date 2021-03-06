'use strict';

var wally = new Wally();
var gui = new GUI();
var date = new Date();
var uv = nucleus.uv;
var wallaby = require('./modules/wally/wallaby');

var file = config.wally.basedir+'/etc/wallyd.d/tests/wallybill.json';
var valsfile = "/tmp/co2.js";

var loopDelay = 10;
var memstartb = wally.getrss();
var memstart = Math.round((memstartb/1024/1024)*100)/100+'mb';

context.privates = {};
context.privates.co2 = 520;
context.privates.co2temp = 24;
context.privates.hrtemp = 25;
context.privates.hr = 44;
context.privates.kw = 23;
context.privates.date = date.getDate()+'.'+date.getMonth()+'.'+date.getFullYear();
context.privates.time = date.getHours()+':'+date.getMinutes();

var page=0;
var data;
try{
    var json = wally.readFile(file);
    data = JSON.parse(json);
} catch(err) {
    log.debug('Error loading and parsing screen file ',file,' : '+err);
}
 
function oninterval() {
   log.debug('Render wallaby');
   if(context.privates.co2 > 800) {
         for(var i = 0; i < data.objects.length; i++){
             var obj = data.objects[i];
             if(obj.value === "CO2" || obj.value === "$_.co2; ppm"){
                 if(obj.style.fontColor){
                    if(context.privates.co2 > 1200) {
                        obj.style.fontColor="#FF6666";
                    } else {
                        obj.style.fontColor="#FFF020";
                    }
                 }
             }
         }
    }
    context.privates.date = date.getDate()+'.'+date.getMonth()+'.'+date.getFullYear();
    context.privates.time = extra.pad(date.getHours(),2)+':'+extra.pad(date.getMinutes(),2)+':'+extra.pad(date.getSeconds(),2);
    var dat = data.pages[page];
    var dat2 = data.pages[page+1];
    if(data.pages.length > 1){
	page++;	
	if(page > data.pages.length-1) { page = 0;}
	if(page > data.pages.length-2) { dat2 =  data.pages[0]; }
    }
    log.info("Presenting page : "+page+" of "+data.pages.length+" with "+dat.objects.length+" elements");
    try {
	var d2 = new Date();
        var start = d2.getTime();
	var passed = 0;
	var TA = new Transaction();
	TA.push(gui.clearTextureNoPaint.bind(null,'main'));
        wallaby.renderScreen(TA,context,context.privates,'main',dat);
        TA.push(wally.render.bind(null,'main'));
	TA.commit();

	passed = d2.getTime()-date.getTime();//-3600*1000;
	var d3 = new Date();
	var fin = d3.getTime();
	d2.setTime(passed);
	var m = extra.pad(d2.getMinutes(),2);
	var h = d2.getHours()-1;
	var d = ~~((passed/1000)/24/3600);
	var uts = h+':'+m+'h';
	if(d >= 1){
	   uts = d+' days '+h+':'+m+'h';
	}
    	var stat = '***   Up: '+uts+
		 ' ***   Render time: '+(fin-start)/1000+'s';
    	wally.log(stat);
    } catch(err) {
	log.error('ERROR: Show status failed : '+err);
    }
}

try {
    var timer = new uv.Timer();
    timer.start( 0, loopDelay, oninterval);
} catch(e) {
    log.error('Error in demo timer : '+e);
}
