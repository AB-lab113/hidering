/* ============================================================
   HIDERING (HRG) — Matrix rain background
   No dependencies. Injects a fixed full-screen canvas behind the
   page content and animates falling glyph columns.
   Honours prefers-reduced-motion and pauses on hidden tabs.
   ============================================================ */
(function () {
  'use strict';

  /* ---- tunables ---- */
  var FONT_SIZE = 16;
  var COL_WIDTH = 16;
  var COLOR = '#00FF41';
  var HEAD_COLOR = '#CCFFCC';
  var VEIL = 'rgba(0,0,0,0.08)';
  var RESET_CHANCE = 0.025;
  var FRAME_MS = 120;
  var FRAME_MS_SMALL = 200;
  var SMALL_WIDTH = 768;

  /* half-width katakana (U+FF66–U+FF9D) + digits + the ticker */
  var GLYPHS = (function () {
    var s = '';
    for (var c = 0xFF66; c <= 0xFF9D; c++) s += String.fromCharCode(c);
    return s + '0123456789HRG';
  })();

  var canvas = document.createElement('canvas');
  canvas.id = 'matrix-bg';
  canvas.setAttribute('aria-hidden', 'true');
  canvas.style.cssText = [
    'position:fixed', 'inset:0', 'width:100%', 'height:100%',
    'z-index:-1', 'pointer-events:none', 'background:#000', 'opacity:0.9'
  ].join(';');

  var ctx = canvas.getContext('2d');
  if (!ctx) return;

  var width = 0, height = 0, columns = 0;
  var drops = [];   /* row index of each column's head */
  var last = [];    /* glyph drawn last frame, redrawn in body colour */

  function glyph() {
    return GLYPHS.charAt(Math.floor(Math.random() * GLYPHS.length));
  }

  function frameInterval() {
    return width < SMALL_WIDTH ? FRAME_MS_SMALL : FRAME_MS;
  }

  /* Size the backing store to the device pixel ratio so glyphs stay
     crisp on HiDPI, then work in CSS pixels via the transform. */
  function resize() {
    var dpr = window.devicePixelRatio || 1;
    width = window.innerWidth;
    height = window.innerHeight;
    canvas.width = Math.floor(width * dpr);
    canvas.height = Math.floor(height * dpr);
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.font = FONT_SIZE + 'px monospace';
    ctx.textBaseline = 'top';

    var next = Math.ceil(width / COL_WIDTH);
    for (var i = columns; i < next; i++) {
      drops[i] = Math.floor(Math.random() * (height / FONT_SIZE));
      last[i] = null;
    }
    drops.length = next;
    last.length = next;
    columns = next;

    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, width, height);
  }

  function draw() {
    /* the veil is what leaves a fading trail behind each head */
    ctx.fillStyle = VEIL;
    ctx.fillRect(0, 0, width, height);
    ctx.font = FONT_SIZE + 'px monospace';

    for (var i = 0; i < columns; i++) {
      var x = i * COL_WIDTH;
      var y = drops[i] * FONT_SIZE;

      /* demote the previous head to the body colour before the new one */
      if (last[i] !== null) {
        ctx.fillStyle = COLOR;
        ctx.fillText(last[i], x, y - FONT_SIZE);
      }

      var ch = glyph();
      ctx.fillStyle = HEAD_COLOR;
      ctx.fillText(ch, x, y);
      last[i] = ch;
      drops[i]++;

      if (y > height && Math.random() < RESET_CHANCE) {
        drops[i] = 0;
        last[i] = null;
      }
    }
  }

  /* One still field, for reduced-motion: no timer is ever started. */
  function drawStatic() {
    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, width, height);
    ctx.font = FONT_SIZE + 'px monospace';

    for (var i = 0; i < columns; i++) {
      var x = i * COL_WIDTH;
      var head = Math.floor(Math.random() * (height / FONT_SIZE));
      var trail = 6 + Math.floor(Math.random() * 13);

      for (var j = trail; j >= 0; j--) {
        var y = (head - j) * FONT_SIZE;
        if (y < -FONT_SIZE || y > height) continue;
        if (j === 0) {
          ctx.fillStyle = HEAD_COLOR;
        } else {
          ctx.globalAlpha = 1 - j / (trail + 1);
          ctx.fillStyle = COLOR;
        }
        ctx.fillText(glyph(), x, y);
        ctx.globalAlpha = 1;
      }
    }
  }

  var reduced = window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)');
  var timer = null;
  var rafId = null;
  var lastFrame = 0;

  function loop(now) {
    rafId = window.requestAnimationFrame(loop);
    if (now - lastFrame < frameInterval()) return;
    lastFrame = now;
    draw();
  }

  function start() {
    if (rafId !== null) return;
    lastFrame = 0;
    rafId = window.requestAnimationFrame(loop);
  }

  function stop() {
    if (rafId === null) return;
    window.cancelAnimationFrame(rafId);
    rafId = null;
  }

  function isReduced() {
    return !!(reduced && reduced.matches);
  }

  function render() {
    if (isReduced()) {
      stop();
      drawStatic();
    } else {
      start();
    }
  }

  document.body.insertBefore(canvas, document.body.firstChild);
  resize();
  render();

  window.addEventListener('resize', function () {
    window.clearTimeout(timer);
    timer = window.setTimeout(function () {
      resize();
      render();
    }, 150);
  });

  document.addEventListener('visibilitychange', function () {
    if (document.hidden) stop();
    else render();
  });

  /* follow a live change of the OS motion preference */
  if (reduced) {
    if (reduced.addEventListener) reduced.addEventListener('change', render);
    else if (reduced.addListener) reduced.addListener(render);
  }
})();
