// Reproduction de l'écran du firmware claude_usage (320 x 172), avec des données de démo.
// Même mise en page et mêmes règles de couleur que firmware/claude_usage/claude_usage.ino.

(function () {
  const W = 320, H = 172, MX = 10;
  const C = {
    bg: '#0a0a0e', track: '#282a34', text: '#ebebf0', dim: '#8c919e', claude: '#d97757',
    grey: '#6e737d', ok: '#22a05a'
  };
  // Nuances des jauges (comme le firmware) : terracotta dans le rythme,
  // vers le rouge en avance sur le rythme, vers le gris chaud en retard (marge).
  const TERRA = [217, 119, 87], RED = [229, 72, 77], CLOUD = [177, 173, 161];
  const DAYS = ['dimanche', 'lundi', 'mardi', 'mercredi', 'jeudi', 'vendredi', 'samedi'];
  const pad = (n) => String(n).padStart(2, '0');

  // Démo : session à mi-parcours, semaine qui se réinitialise jeudi à 18 h.
  function demoState(now) {
    const fiveReset = new Date(now.getTime() + (2 * 60 + 31) * 60000);
    fiveReset.setSeconds(0, 0);
    const weekReset = new Date(now);
    weekReset.setHours(18, 0, 0, 0);
    let add = (4 - weekReset.getDay() + 7) % 7;
    if (add === 0 && weekReset <= now) add = 7;
    weekReset.setDate(weekReset.getDate() + add);
    return {
      five: { used: 47, reset: fiveReset, hours: 5 },
      week: { used: 29, reset: weekReset, hours: 168 }
    };
  }

  function frac(g, now) {
    const span = g.hours * 3600000;
    const f = (now - (g.reset - span)) / span;
    return Math.min(1, Math.max(0, f));
  }

  function gaugeRGB(g, f) {
    const gap = g.used - 100 * f;
    const t = Math.min(1, Math.max(0, (Math.abs(gap) - 10) / 25));
    const to = gap > 0 ? RED : CLOUD;
    return TERRA.map((v, i) => Math.round(v + (to[i] - v) * t));
  }

  function gaugeColor(g, f) {
    const [r, gg, b] = gaugeRGB(g, f);
    return `rgb(${r},${gg},${b})`;
  }

  // « 21:00 » aujourd'hui, « demain 02:40 », sinon « jeudi 18:00 »
  function fmtReset(d, now) {
    const day = (x) => Math.floor((x.getTime() - x.getTimezoneOffset() * 60000) / 86400000);
    const diff = day(d) - day(now);
    const hm = pad(d.getHours()) + ':' + pad(d.getMinutes());
    if (diff <= 0) return hm;
    if (diff === 1) return 'demain ' + hm;
    return DAYS[d.getDay()] + ' ' + hm;
  }

  function roundRect(ctx, x, y, w, h, r) {
    ctx.beginPath();
    ctx.roundRect(x, y, w, h, Math.min(r, w / 2, h / 2));
    ctx.fill();
  }

  function drawGauge(ctx, top, label, g, now) {
    const f = frac(g, now);
    const col = gaugeColor(g, f);
    ctx.textBaseline = 'alphabetic';
    ctx.font = 'bold 13px Arial, Helvetica, sans-serif';
    ctx.fillStyle = C.dim;
    ctx.textAlign = 'left';
    ctx.fillText(label, MX, top + 14);
    ctx.font = 'bold 18px Arial, Helvetica, sans-serif';
    ctx.fillStyle = col;
    ctx.textAlign = 'right';
    ctx.fillText(g.used + '%', W - MX, top + 17);

    const bx = MX, by = top + 22, bw = W - 2 * MX, bh = 16;
    ctx.fillStyle = C.track;
    roundRect(ctx, bx, by, bw, bh, 5);
    ctx.fillStyle = col;
    roundRect(ctx, bx, by, bw * g.used / 100, bh, 5);
    const mx = Math.round(bx + f * (bw - 1));
    ctx.fillStyle = C.bg;
    ctx.fillRect(mx - 1, by - 3, 3, bh + 6);
    ctx.fillStyle = C.text;
    ctx.fillRect(mx, by - 3, 1.2, bh + 6);

    ctx.font = '13px Arial, Helvetica, sans-serif';
    ctx.fillStyle = C.dim;
    ctx.textAlign = 'left';
    ctx.fillText(fmtReset(g.reset, now), MX, by + bh + 16);
  }

  // Dessine l'écran dans un contexte 2D de taille (320*scale) x (172*scale).
  window.drawMiniScreen = function (ctx, scale, now) {
    now = now || new Date();
    const st = demoState(now);
    ctx.save();
    ctx.setTransform(scale, 0, 0, scale, 0, 0);
    ctx.fillStyle = C.bg;
    ctx.fillRect(0, 0, W, H);

    ctx.textBaseline = 'alphabetic';
    ctx.textAlign = 'left';
    ctx.font = 'bold 19px Arial, Helvetica, sans-serif';
    ctx.fillStyle = C.claude;
    ctx.fillText('Claude', MX, 22);
    const tw = ctx.measureText('Claude').width;
    ctx.font = '14px Arial, Helvetica, sans-serif';
    ctx.fillStyle = C.dim;
    ctx.fillText('usage', MX + tw + 6, 22);

    ctx.fillStyle = C.ok;
    ctx.beginPath();
    ctx.arc(W - MX - 6, 15, 6, 0, Math.PI * 2);
    ctx.fill();

    drawGauge(ctx, 32, 'SESSION 5 H', st.five, now);
    drawGauge(ctx, 94, 'SEMAINE', st.week, now);

    ctx.font = '8px "JetBrains Mono", Consolas, monospace';
    ctx.fillStyle = C.grey;
    ctx.textAlign = 'left';
    const upd = new Date(now.getTime() - 60000);
    ctx.fillText('Releve a ' + pad(upd.getHours()) + ':' + pad(upd.getMinutes()), MX, H - 4);
    ctx.restore();
    const [r, g, b] = gaugeRGB(st.five, frac(st.five, now));
    return (r << 16) | (g << 8) | b;   // couleur de la LED
  };

  window.MINI_SCREEN = { W, H };
})();
