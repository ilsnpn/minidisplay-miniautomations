// Pages de documentation : coloration du code, schémas Mermaid, sommaire actif, boutons « Copier ».
(function () {
  // Coloration syntaxique (highlight.js)
  if (window.hljs) document.querySelectorAll('pre code').forEach((el) => window.hljs.highlightElement(el));

  // Boutons « Copier » sur chaque bloc de code
  document.querySelectorAll('.code').forEach((block) => {
    const btn = block.querySelector('.copy-btn');
    const code = block.querySelector('pre code');
    if (!btn || !code) return;
    btn.addEventListener('click', async () => {
      try {
        await navigator.clipboard.writeText(code.innerText);
        btn.textContent = 'Copié';
      } catch (e) {
        btn.textContent = 'Échec';
      }
      setTimeout(() => { btn.textContent = 'Copier'; }, 1400);
    });
  });

  // Schémas Mermaid, aux couleurs du site
  if (window.mermaid) {
    window.mermaid.initialize({
      startOnLoad: true,
      theme: 'base',
      securityLevel: 'strict',
      fontFamily: 'Inter, system-ui, sans-serif',
      themeVariables: {
        darkMode: true,
        background: '#101016',
        primaryColor: '#1b1b24',
        primaryTextColor: '#ececf1',
        primaryBorderColor: '#3a3a48',
        secondaryColor: '#15151c',
        tertiaryColor: '#101016',
        lineColor: '#8a8a98',
        textColor: '#ececf1',
        fontSize: '14px',
        actorBkg: '#1b1b24',
        actorBorder: '#d97757',
        actorTextColor: '#ececf1',
        actorLineColor: '#3a3a48',
        signalColor: '#b8b8c4',
        signalTextColor: '#ececf1',
        labelBoxBkgColor: '#1b1b24',
        labelBoxBorderColor: '#3a3a48',
        labelTextColor: '#ececf1',
        loopTextColor: '#ececf1',
        noteBkgColor: '#2a1d17',
        noteTextColor: '#f0c3b2',
        noteBorderColor: '#d97757',
        activationBkgColor: '#2a1d17',
        activationBorderColor: '#d97757',
        sequenceNumberColor: '#0b0b0f',
        edgeLabelBackground: '#15151c',
        clusterBkg: '#13131a',
        clusterBorder: '#2a2a36',
        stateLabelColor: '#ececf1',
        transitionColor: '#8a8a98',
        transitionLabelColor: '#ececf1',
        // Thème clair du système : mêmes rôles, couleurs claires
        ...((window.mdmaIsLight ? window.mdmaIsLight() : matchMedia('(prefers-color-scheme: light)').matches) ? {
          darkMode: false, background: '#f3f2ee', primaryColor: '#ffffff', primaryTextColor: '#1c1c21',
          primaryBorderColor: '#cfcdc5', secondaryColor: '#f4f3ef', tertiaryColor: '#f3f2ee', lineColor: '#8a8a94',
          textColor: '#1c1c21', actorBkg: '#ffffff', actorBorder: '#c2603f', actorTextColor: '#1c1c21',
          actorLineColor: '#cfcdc5', signalColor: '#5b5b65', signalTextColor: '#1c1c21', labelBoxBkgColor: '#ffffff',
          labelBoxBorderColor: '#cfcdc5', labelTextColor: '#1c1c21', loopTextColor: '#1c1c21', noteBkgColor: '#fbeee8',
          noteTextColor: '#7a3520', noteBorderColor: '#c2603f', activationBkgColor: '#fbeee8', activationBorderColor: '#c2603f',
          sequenceNumberColor: '#ffffff', edgeLabelBackground: '#ffffff', clusterBkg: '#f7f6f2', clusterBorder: '#e4e2db',
          stateLabelColor: '#1c1c21', transitionColor: '#8a8a94', transitionLabelColor: '#1c1c21'
        } : {})
      }
    });
  }

  // Sommaire : surligne la section en cours de lecture
  const links = Array.from(document.querySelectorAll('.toc a'));
  const targets = links.map((a) => document.querySelector(a.getAttribute('href'))).filter(Boolean);
  if (targets.length) {
    const io = new IntersectionObserver((entries) => {
      entries.forEach((e) => {
        if (!e.isIntersecting) return;
        links.forEach((a) => a.classList.toggle('active', a.getAttribute('href') === '#' + e.target.id));
      });
    }, { rootMargin: '-80px 0px -70% 0px' });
    targets.forEach((t) => io.observe(t));
  }
})();
