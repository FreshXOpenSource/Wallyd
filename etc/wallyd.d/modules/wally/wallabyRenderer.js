'use strict';

//var qr = require('modules/qr/qr');
var uv = nucleus.uv;

if(!context.compatMode){
   var gui = new GUI();
   var wally = new Wally();
   var config = wally.getConfig();
   var svg = new SVG();
}else{
   log.warn('Running wallaby renderer in compat mode. Screen commands will be printed.');
   var wally = Wally;
   var gui = Wally;
   var config = context.config;
}

function parseString(str){
    if( str === undefined ) return "";
    var matchTable={};
    var splitter = /\$_.[A-Za-z0-9_]*;/g;
    var match = str.match(splitter);
    if(match === null) return str;
    for (var j = 0; j<match.length; j++)
    {
        var smatch = match[j].replace(/\$_./,'').replace(/;$/,'');
        //log.debug(smatch);
        if(context.privates && context.privates[smatch]){
             var val = context.privates[smatch].toString();
             matchTable[smatch] = val;
         } else {
            log.error("Key ",smatch," not found in context.privates");
            return "undefined";
        }
    }
    var destVal = str;
    for (var k in matchTable){
        var re=RegExp('._.'+k+';');
        var t = destVal.replace(re,matchTable[k]);
        destVal = t;
    }
    return destVal;
}

function findMin(data){
   var x = ~~(data.objects[0].options.geometry.x);
   var y = ~~(data.objects[0].options.geometry.y);
   for(var i = 1; i < data.objects.length; i++){
        var obj = data.objects[i];
	var opts = obj.options;
	var value = obj.value;
        var ox = ~~(opts.geometry.x);
        var oy = ~~(opts.geometry.y);
        if(x > ox) x = ox;
        if(y > oy) y = oy;
   }
   return [-x,-y];
}

function renderScreen(TA, server, context, tree, screen, data)
{
   var json;
   var maxWidth=0, maxHeight=0;
   var xScale=1.0, yScale=1.0;
   var r = findMin(data);
   var rX=r[0], rY=r[1];
   var start = new Date().getTime();

   //config = context.config.wally;

   log.info("Setup wallaby destination : "+screen)
   TA.push(wally.setAutoRender.bind(null,false));
   TA.push(gui.setTargetTexture.bind(null,screen));

   var width = data._options.width ? data._options.width : data._options.size[0];
   var height = data._options.height ? data._options.height : data._options.size[1];
   if(width === 0 || height === 0){
      log.error("No valid dimensions found in screen");
      return 0;
   } else {
      xScale = (config.width-10)/width;
      yScale = (config.height-30)/height;
      log.info("Dimensions in document : "+width+"x"+height+" / Scaling : "+xScale+"x"+yScale+" / Reloc "+ rX + "x" + rY);
   }
    
   for(var i = 0; i < data.objects.length; i++){
        var obj = data.objects[i];
	var opts = obj.options;
	var style = obj.options.style;
	var value = obj.value;
        var X = ~~((opts.geometry.x+rX)*xScale);
        var Y = ~~((opts.geometry.y+rY)*yScale);
        var W = 0;
        var H = 0;
        var color = 0;
        var alpha = 255;
        var fillColor = 0;
        var gradientColor = undefined;
        if(opts.geometry.width){
            W = ~~(opts.geometry.width*xScale);
        }
        if(opts.geometry.height){
            H = ~~(opts.geometry.height*yScale);
        }
        if(opts.style.strokeColor) {
            color= parseInt(opts.style.strokeColor.replace('#','0x'));
        }
        if(opts.style.fontColor) {
            color= parseInt(opts.style.fontColor.replace('#','0x'));
        }
        if(opts.style.fillColor) {
            fillColor= parseInt(opts.style.fillColor.replace('#','0x'));
        } else {
            alpha = 0;
        }
        if(opts.style.gradientColor) {
            gradientColor = parseInt(opts.style.gradientColor.replace('#','0x'));
        }
        log.debug('WR : ',obj.type,screen,X, Y, W/xScale, H/yScale, 255);

        if(obj.type === 'image'){
            // TODO : request.js and put into TA
            var imgUrl = obj.path.replace(/^undefined/,server);
            var res = request(imgUrl, function(err, header, data){
               if(data){
                  wally.writeFileSync('/tmp/test.png',data);
                  TA.push(gui.loadImage.bind(null,screen,'/tmp/test.png',X, Y, W/xScale, H/yScale, 255));
               } else {
                  log.error('Could not download from '+server+': '+imgUrl);
               }
               return;
           });
           continue;
        }
   
        if(obj.type === 'image2'){
	    if(!curl) {
		continue;
	    }
            // TODO : request.js and put into TA
            var imgUrl = obj.path.replace(/^undefined/,server);
            var res = curl.get(imgUrl);
            if(res.body){
               wally.writeFileSync('/tmp/test.png',res.body);
               TA.push(gui.loadImage.bind(null,screen,'/tmp/test.png',X, Y, W/xScale, H/yScale, 255));
            } else {
            //TA.push(gui.loadImage.bind(null,screen,'/tmp/test.png',X, Y, W, H, 255));
               log.error('Could not download from '+server+': '+imgUrl);
            }
            continue;
        }
        if(obj.type === 'line'){
            log.debug(obj);
            if(style.direction && style.direction === 'south'){
               TA.push(gui.drawLine.bind(null,screen,X, Y, X , Y + H, 1));
            } else {
               TA.push(gui.drawLine.bind(null,screen,X, Y, X + W, Y, 1));
            }
            continue;
	}
	// TODO : do we draw boxes without an edge / edge = 0?
        if(obj.type === 'rect'){
	    if(opts.edge && opts.edge > 0 ){
	       log.debug(screen, X, Y, W, H, parseInt(opts.edge), fillColor, color, alpha);
	       TA.push(gui.drawFilledBox.bind(null,screen, X, Y, W, H, parseInt(opts.edge), fillColor, color, alpha));
	    } else {
	       log.debug(screen, X, Y, W, H, 1, fillColor, color, alpha);
	       TA.push(gui.drawFilledBox.bind(null,screen, X, Y, W, H, 1, fillColor, color, alpha));
            
            }
            continue;
	}
        if(obj.type === 'qr'){
	    log.debug('QR not yet ported');
	    continue;
            var qrtext = parseString(opts.value.replace('qr:',''));
            //var res = qr.image(qrtext, { type: 'svg' });
            //log.debug(res);
            //var res = qr.file(qrtext,'/tmp/qr.svg', { type: 'svg' });
            svg.svgToPng("/tmp/qr.svg","/tmp/qr.png");
            log.debug('Placing QR Code at : ',X,Y,W,H);
            TA.push(gui.loadImage.bind(null,screen,'/tmp/qr.png',X,Y,W,H,255));
            //var ptr = svg.svgToImage(res.toString());
            //var preview = Array.prototype.slice.call(ptr, 0, ptr.length).map(function (byte) {
            //    return byte < 16 ? "0" + byte.toString(16) : byte.toString(16);
            //}).join(" ");
            //log.debug(preview);
            //gui.putImage(screen,ptr,X,Y,W,H,255);
            ////log.debug(preview);
            //gui.putImage(screen,ptr,X,Y,W,H,255);

            continue;
	}

        if(obj.type === 'text'){
	    if(W === 0 || H === 0 || value === undefined || value.length === 0){
		 continue;
	    }
            if(style.fontSize){
               var fontSize = style.fontSize;
            } else {
               var fontSize = H;
            }
            var fName = 'font'+fontSize;
            var val = "";
            TA.push(wally.loadFont.bind(null,fName, config.basedir+'/etc/wallyd.d/fonts/Lato-Bol.ttf', fontSize));
            if(value.match(/\$_./)){
               var destVal = parseString(value);
               TA.push(gui.drawText.bind(null,screen,X, Y, fName, color, destVal));
            } else {
               TA.push(gui.drawText.bind(null,screen,X, Y, fName, color, value));
            }
	    continue;
	}
        log.debug('Dont know how to handle type '+obj.type+' yet.');
    }
  
    var end = new Date().getTime()-start;
    log.debug({'time': end /1000 });
//    return end;
//    log.debug('Wallaby Screen has max size '+maxWidth+'x'+maxHeight+' Scaling by '+xScale+'x'+yScale+' Relocating by '+rX+'/'+rY);
}

exports.renderScreen = renderScreen;
