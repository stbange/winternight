// Winternight web glue (Emscripten --js-library): WebXR bridge (spec webxr-mode).
// The game renders each eye itself; this file only talks to navigator.xr.
addToLibrary({
  wn_web_xr_init__deps: ['$GL'],
  wn_web_xr_init: function () {
    var button = document.getElementById('vr');
    if (!navigator.xr || !button) return;
    navigator.xr.isSessionSupported('immersive-vr').then(function (supported) {
      if (supported) button.style.display = 'block';
    }).catch(function () {});

    window.wnEnterVR = async function () {
      var gl = GL.currentContext && GL.currentContext.GLctx;
      if (!gl) return false;
      if (gl.makeXRCompatible) await gl.makeXRCompatible();
      var session = await navigator.xr.requestSession('immersive-vr', { optionalFeatures: ['local-floor'] });
      var layer = new XRWebGLLayer(session, gl);
      session.updateRenderState({ baseLayer: layer });
      var space;
      try { space = await session.requestReferenceSpace('local-floor'); }
      catch (e) { space = await session.requestReferenceSpace('local'); }
      var views = _malloc(4 * 32 * 4), viewports = _malloc(4 * 4 * 4), inputs = _malloc(4 * 8);
      var fboId = 0, lastFbo = null, last = performance.now(), lastHand = null;
      window.WN_XR_SESSION = session;
      _wn_xr_session_started();
      function onFrame(t, frame) {
        session.requestAnimationFrame(onFrame);
        var pose = frame.getViewerPose(space);
        if (!pose) return;
        var fb = layer.framebuffer;
        if (fb !== lastFbo) {
          // Register the XR framebuffer with Emscripten's GL id table.
          if (fb) { fboId = GL.getNewId(GL.framebuffers); fb.name = fboId; GL.framebuffers[fboId] = fb; }
          else fboId = 0;
          lastFbo = fb;
        }
        var n = Math.min(pose.views.length, 4);
        for (var i = 0; i < n; i++) {
          var v = pose.views[i];
          HEAPF32.set(v.transform.inverse.matrix, (views >> 2) + i * 32);
          HEAPF32.set(v.projectionMatrix, (views >> 2) + i * 32 + 16);
          var vp = layer.getViewport(v);
          HEAP32.set([vp.x, vp.y, vp.width, vp.height], (viewports >> 2) + i * 4);
        }
        // Controllers: left stick moves, right stick turns, trigger/grip/A, swing speed of the right hand.
        var inp = [0, 0, 0, 0, 0, 0, 0, 0];
        session.inputSources.forEach(function (src) {
          var gp = src.gamepad;
          if (gp) {
            if (src.handedness === 'left') { inp[0] = gp.axes[2] || 0; inp[1] = gp.axes[3] || 0; }
            if (src.handedness === 'right') {
              inp[2] = gp.axes[2] || 0;
              inp[3] = gp.buttons[0] ? gp.buttons[0].value : 0;
              inp[4] = gp.buttons[1] ? gp.buttons[1].value : 0;
              inp[5] = gp.buttons[4] && gp.buttons[4].pressed ? 1 : 0;
            }
          }
          if (src.handedness === 'right' && src.gripSpace) {
            var hp = frame.getPose(src.gripSpace, space);
            if (hp) {
              var p = hp.transform.position;
              if (lastHand) {
                var dt = Math.max((t - last) / 1000, 1e-3);
                var dx = p.x - lastHand.x, dy = p.y - lastHand.y, dz = p.z - lastHand.z;
                var speed = Math.sqrt(dx * dx + dy * dy + dz * dz) / dt;
                if (speed > 2.2) inp[6] = speed;
              }
              lastHand = { x: p.x, y: p.y, z: p.z };
            }
          }
        });
        HEAPF32.set(inp, inputs >> 2);
        var dts = Math.min((t - last) / 1000, 0.25);
        last = t;
        _wn_xr_frame(views, viewports, n, fboId, layer.framebufferWidth, layer.framebufferHeight, inputs, dts);
      }
      session.requestAnimationFrame(onFrame);
      session.addEventListener('end', function () {
        _wn_xr_session_ended();
        _free(views); _free(viewports); _free(inputs);
        window.WN_XR_SESSION = null;
      });
      return true;
    };
  },
});
