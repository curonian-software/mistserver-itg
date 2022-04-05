mistplayers.html5={name:"HTML5 video player",mimes:["html5/application/vnd.apple.mpegurl","html5/application/vnd.apple.mpegurl;version=7","html5/video/mp4","html5/video/ogg","html5/video/webm","html5/audio/mp3","html5/audio/webm","html5/audio/ogg","html5/audio/wav"],priority:MistUtil.object.keys(mistplayers).length+1,isMimeSupported:function(e){return MistUtil.array.indexOf(this.mimes,e)==-1?false:true},isBrowserSupported:function(e,t,i){if(location.protocol!=MistUtil.http.url.split(t.url).protocol){if(location.protocol=="file:"&&MistUtil.http.url.split(t.url).protocol=="http:"){i.log("This page was loaded over file://, the player might not behave as intended.")}else{i.log("HTTP/HTTPS mismatch for this source");return false}}if(e=="html5/application/vnd.apple.mpegurl"){var r=MistUtil.getAndroid();if(r&&parseFloat(r)<7){i.log("Skipping native HLS as videojs will do better");return false}}var a=false;var n=e.split("/");n.shift();try{n=n.join("/");function s(e){var t=document.createElement("video");if(t&&t.canPlayType(e)!=""){a=t.canPlayType(e)}return a}function o(e){function t(t){return("0"+e.init.charCodeAt(t).toString(16)).slice(-2)}switch(e.codec){case"AAC":return"mp4a.40.2";case"MP3":return"mp3";case"AC3":return"ec-3";case"H264":return"avc1."+t(1)+t(2)+t(3);case"HEVC":return"hev1."+t(1)+t(6)+t(7)+t(8)+t(9)+t(10)+t(11)+t(12);default:return e.codec.toLowerCase()}}var l={};for(var p in i.info.meta.tracks){if(i.info.meta.tracks[p].type!="meta"){l[o(i.info.meta.tracks[p])]=1}}l=MistUtil.object.keys(l);if(n=="video/mp4"){if(l.length){if(l.length>t.simul_tracks){var u=0;for(var p in l){var f=s(n+';codecs="'+l[p]+'"');if(f){u++}}return u>=t.simul_tracks}return s(n+';codecs="'+l.join(",")+'"')}}else{for(var p=l.length-1;p>=0;p--){if(l[p].substr(0,4)=="hev1"){l.splice(p,1)}}if(l.length<t.simul_tracks){return false}}a=s(n)}catch(e){}return a},player:function(){this.onreadylist=[]},mistControls:true};var p=mistplayers.html5.player;p.prototype=new MistPlayer;p.prototype.build=function(e,t){var i=e.source.type.split("/");i.shift();var r=document.createElement("video");r.setAttribute("crossorigin","anonymous");r.setAttribute("playsinline","");var a=document.createElement("source");a.setAttribute("src",e.source.url);r.source=a;r.appendChild(a);a.type=i.join("/");var n=["autoplay","loop","poster"];for(var s in n){var o=n[s];if(e.options[o]){r.setAttribute(o,e.options[o]===true?"":e.options[o])}}if(e.options.muted){r.muted=true}if(e.options.controls=="stock"){r.setAttribute("controls","")}if(e.info.type=="live"){r.loop=false}if("Proxy"in window&&"Reflect"in window){var l={get:{},set:{}};e.player.api=new Proxy(r,{get:function(e,t,i){if(t in l.get){return l.get[t].apply(e,arguments)}var r=e[t];if(typeof r==="function"){return function(){return r.apply(e,arguments)}}return r},set:function(e,t,i){if(t in l.set){return l.set[t].call(e,i)}return e[t]=i}});if(e.source.type=="html5/audio/mp3"){l.set.currentTime=function(){e.log("Seek attempted, but MistServer does not currently support seeking in MP3.");return false}}if(e.info.type=="live"){l.get.duration=function(){var t=0;if(this.buffered.length){t=this.buffered.end(this.buffered.length-1)}var i=((new Date).getTime()-e.player.api.lastProgress.getTime())*.001;return t+i-e.player.api.liveOffset};l.set.currentTime=function(t){var i=t-e.player.api.duration;if(i>0){i=0}e.player.api.liveOffset=i;e.log("Seeking to "+MistUtil.format.time(t)+" ("+Math.round(i*-10)/10+"s from live)");var r={startunix:i};if(i==0){r={}}e.player.api.setSource(MistUtil.http.url.addParam(e.source.url,r))};MistUtil.event.addListener(r,"progress",function(){e.player.api.lastProgress=new Date});e.player.api.lastProgress=new Date;e.player.api.liveOffset=0;MistUtil.event.addListener(r,"pause",function(){e.player.api.pausedAt=new Date});l.get.play=function(){return function(){if(e.player.api.paused&&e.player.api.pausedAt&&new Date-e.player.api.pausedAt>5e3){r.load();e.log("Reloading source..")}return r.play.apply(r,arguments)}};if(e.source.type=="html5/video/mp4"){var p=l.get.duration;l.get.duration=function(){return p.apply(this,arguments)-e.player.api.liveOffset+e.info.lastms*.001};l.get.currentTime=function(){return this.currentTime-e.player.api.liveOffset+e.info.lastms*.001};l.get.buffered=function(){var t=this;return{length:t.buffered.length,start:function(i){return t.buffered.start(i)-e.player.api.liveOffset+e.info.lastms*.001},end:function(i){return t.buffered.end(i)-e.player.api.liveOffset+e.info.lastms*.001}}}}}else{if(!isFinite(r.duration)){var u=0;for(var s in e.info.meta.tracks){u=Math.max(u,e.info.meta.tracks[s].lastms)}l.get.duration=function(){if(isFinite(this.duration)){return this.duration}return u*.001}}}}else{e.player.api=r}e.player.api.setSource=function(e){if(e!=this.source.src){this.source.src=e;this.load()}};e.player.api.setSubtitle=function(e){var t=r.getElementsByTagName("track");for(var i=t.length-1;i>=0;i--){r.removeChild(t[i])}if(e){var a=document.createElement("track");r.appendChild(a);a.kind="subtitles";a.label=e.label;a.srclang=e.lang;a.src=e.src;a.setAttribute("default","")}};e.player.setSize=function(e){this.api.style.width=e.width+"px";this.api.style.height=e.height+"px"};t(r)};