function Transaction() {
  this.initialize();
}

Transaction.prototype = {

    ID:		0,
    funcs:	null,

    initialize: function() {
	this.funcs = [];
	if(typeof(CTransaction) === 'undefined'){
	  log.warn('CTransactions not available.');
	  this.ID = 15;
	  this.ct = {
	    lock:   function(a){ log.screen('LOCK : ',a);},
	    commit: function(a){ log.screen('COMMIT : ',a);},
	    clear:  function(a){ log.screen('CLEAR : ',a);}
	  };
	} else {
	  this.ct = new CTransaction();
	  this.ID = this.ct.newTransaction();
	}
    },

    toString:   function() {
        return '{ size : '+this.funcs.length+', id : '+this.ID+' }';
    },

    push: function(f){
	this.funcs.push(f);
    },

    abort: function(){
	delete funcs;
    },

    commit: function(){
    	log.debug('Commiting transaction '+this.ID+' with '+this.funcs.length+' elements');
	if(this.ct){
	    this.ct.lock(this.ID);
	    // All other transactions are blocked while this loop is running!
	    for(var i = 0; i < this.funcs.length; i++){
	      this.funcs[i]();
	    }
	    this.ct.commit(this.ID);
	}
	delete funcs;
	this.ct.clear(this.ID);
    },
}; 
