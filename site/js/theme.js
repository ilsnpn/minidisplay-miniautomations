// Thème du site : Auto (suit le système, par défaut), Clair ou Sombre.
// Chargé dans <head> pour appliquer le choix avant l'affichage, sans flash.
(function () {
  const KEY = 'mdma.theme';
  const MODES = ['auto', 'light', 'dark'];
  const LABEL = { auto: 'Auto', light: 'Clair', dark: 'Sombre' };
  const ICON = {
    auto: '<circle cx="12" cy="12" r="8"/><path d="M12 4v16" /><path d="M12 4a8 8 0 0 1 0 16z" fill="currentColor"/>',
    light: '<circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M2 12h2M20 12h2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/>',
    dark: '<path d="M20 14.5A8 8 0 1 1 9.5 4a6.5 6.5 0 0 0 10.5 10.5z"/>'
  };
  let mode = 'auto';
  try { mode = localStorage.getItem(KEY) || 'auto'; } catch (e) { /* stockage indisponible */ }
  if (!MODES.includes(mode)) mode = 'auto';

  function apply() {
    if (mode === 'auto') delete document.documentElement.dataset.theme;
    else document.documentElement.dataset.theme = mode;
  }
  apply();

  // Thème réellement affiché, pour les scripts (schémas Mermaid)
  window.mdmaIsLight = () => mode === 'light' || (mode === 'auto' && matchMedia('(prefers-color-scheme: light)').matches);

  document.addEventListener('DOMContentLoaded', () => {
    const wrap = document.querySelector('.nav .wrap');
    if (!wrap) return;
    const b = document.createElement('button');
    b.type = 'button';
    b.className = 'theme-switch';
    const render = () => {
      b.innerHTML = `<svg viewBox="0 0 24 24" aria-hidden="true">${ICON[mode]}</svg>${LABEL[mode]}`;
      b.title = 'Thème : ' + LABEL[mode] + ' (cliquer pour changer)';
    };
    render();
    b.addEventListener('click', () => {
      mode = MODES[(MODES.indexOf(mode) + 1) % MODES.length];
      try { mode === 'auto' ? localStorage.removeItem(KEY) : localStorage.setItem(KEY, mode); } catch (e) { /* idem */ }
      apply();
      render();
      if (document.querySelector('.mermaid')) location.reload();  // les schémas se dessinent une seule fois
    });
    wrap.appendChild(b);
  });
})();
