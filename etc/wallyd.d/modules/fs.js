(function(){
"use strict";

var BUFFER_SIZE = 8;
var uv = nucleus.uv;

function readdir(path,callback){
  nucleus.scandir(path, function (req){
    if(!req){
        print('ERROR in scandir');
        return callback('error',null);
    }
    var files = {};
    var i = 1;
    while(true){
      var ent = uv.fs_scandir_next(req);
      if(!ent) {
        break;
      }
      if(ent.type === "directory"){
        files[i] = ent.name;
      } else {
        files[i] = ent;
      }
      i++;
    }
    callback(null,files);
  });
}

function rethrow ( ) {
  return function (err) {
    if (err) {
      throw err;  // Forgot a callback
    }
  };
}

function maybeCallback (cb) {
  return typeof cb === 'function' ? cb : rethrow();
}

function open(path, flags, mode_) {
  var mode = typeof mode_ === 'number' ? mode_ : 438 /* 0666 */;
  var callback = maybeCallback(arguments[arguments.length - 1]);

  uv.fs_open(path, flags, mode, function (err, fd) {
    if (err) {
      callback(err);
    } else {
      callback(undefined, fd);
    }
  });
}

function openSync (path, flags, mode_) {
  var mode = typeof mode_ === 'number' ? mode_ : 438 /* 0666 */;
  var fd;

  try {
    fd = uv.fs_open(path, flags, mode);
  } catch (err) {
    return callback(err);
  }

  callback(undefined, fd);
}

function readFileSync (filename, encoding_) {
  var encoding = encoding_ ? encoding_ : 'utf8';

  var fd = uv.fs_open(filename, 'r', 438); // 438 = 0666
  var buf;
  var count = 0;
  var r;

  while ((r = uv.fs_read(fd, BUFFER_SIZE, (BUFFER_SIZE * count))) !== undefined) {
    count++;
    if (r.length === 0) {
      break;
    } else {
      if (buf === undefined) {
        if (encoding === 'utf8') {
          buf = r;
        } else {
          buf = Duktape.Buffer(r);
        }
      } else {
        if (encoding === 'utf8') {
          buf += r;
        } else {
          buf = Duktape.Buffer(buf + r);
        }
      }
    }
  }

  uv.fs_close(fd);

  return buf;
}

function readFile (filename, callback) {
  var fd;
  var buf;
  var count = 0;

  try {
    fd = uv.fs_open(filename, 'r', 438); // 438 = 0666
  } catch (err) {
    return callback(err);
  }

  function cb (data) {
    if (data.length === 0) {
      uv.fs_close(fd);
      callback(undefined, buf);
    } else {
      count++;
      buf += data;

      uv.fs_read(fd, BUFFER_SIZE, (BUFFER_SIZE * count), cb);
    }
  }

  uv.fs_read(fd, BUFFER_SIZE, (BUFFER_SIZE * count), cb);
}

function exists (path, callback) {
  try {
    uv.fs_stat(path);
  } catch (err) {
    callback(undefined, false);
  }
  callback(undefined, true);
}

function existsSync (path) {
  try {
    uv.fs_stat(path);
  } catch (err) {
    return false;
  }

  return true;
}

function writeFileSync (filename, buf, encoding_) {
  var encoding = encoding_ ? encoding_ : 'utf8';

  var fd = uv.fs_open(filename, 'w', 438); // 438 = 0666
  uv.fs_write(fd, buf, -1);
  uv.fs_close(fd);
  return buf;
}

function writeFile (filename, buf, callback) {
  var fd;
  var count = 0;

  try {
    fd = uv.fs_open(filename, 'w', 438); // 438 = 0666
  } catch (err) {
    return callback(err);
  }

  return uv.fs_write(fd, buf, 0, callback);
}

return {
    //stderr: new uv.Tty(2, false),
    //stdin:  new uv.Tty(0, true),
    //stdout: new uv.Tty(1, false),

    readdir:  readdir,
    readFile: readFile,
    readFileSync: readFileSync,
    exists: exists,
    existsSync: existsSync,
    writeFileSync: writeFileSync,
    writeFile: writeFile
};

})();
