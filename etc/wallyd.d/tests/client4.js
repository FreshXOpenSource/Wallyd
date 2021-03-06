nucleus.dofile('modules/bootstrap.js');
var request = nucleus.dofile('modules/request.js').request;
var log = nucleus.dofile('modules/log.js');

function getImage(url){
    log.debug('Requesting url ',url);
    request(url, function(err,header,body){
        if(err) {
           log.error('Could not connect to server : '+err);
           return(err);
        }
        try {
           //var str = body.toString('utf-8');
           log.info('H:',header);
           log.info('B:',body.length);
        } catch(e) {
           log.error('Could not decode server response : ',e);
        }
        log.info('request done.');
       // print(body);
    });
    log.info('getImage done.');
}

getImage('http://127.0.0.1:4444/scan/wally:0815/_screen:reporting--shared/helo?report=7f97fe8f-05ec-4dee-9b22-cec748d0e9bd&action=output-revolution-json');

nucleus.uv.run();
