'use strict';
const root = document.documentElement;
const themeButton = document.querySelector('.theme-toggle');
const systemTheme = window.matchMedia('(prefers-color-scheme: dark)');
function savedTheme() {
  try { return localStorage.getItem('void-theme'); } catch { return null; }
}
function applyTheme(theme) {
  root.dataset.theme = theme;
  themeButton.setAttribute('aria-pressed', String(theme === 'dark'));
  themeButton.setAttribute('aria-label', `Switch to ${theme === 'dark' ? 'light' : 'dark'} theme`);
}
const saved = savedTheme();
applyTheme(saved === 'dark' || saved === 'light' ? saved : systemTheme.matches ? 'dark' : 'light');
themeButton.hidden = false;
themeButton.addEventListener('click', () => {
  const theme = root.dataset.theme === 'dark' ? 'light' : 'dark';
  applyTheme(theme);
  try { localStorage.setItem('void-theme', theme); } catch { /* Theme still works without storage. */ }
});
systemTheme.addEventListener('change', event => {
  if (!['dark', 'light'].includes(savedTheme())) applyTheme(event.matches ? 'dark' : 'light');
});
const menuButton = document.querySelector('.menu-toggle');
const nav = document.querySelector('#navigation');
function closeMenu() {
  nav.dataset.open = 'false';
  menuButton.setAttribute('aria-expanded', 'false');
}
closeMenu();
menuButton.hidden = false;
menuButton.addEventListener('click', () => {
  const open = menuButton.getAttribute('aria-expanded') !== 'true';
  nav.dataset.open = String(open);
  menuButton.setAttribute('aria-expanded', String(open));
});
nav.addEventListener('click', event => {
  if (event.target.closest('a')) closeMenu();
});
document.addEventListener('keydown', event => {
  if (event.key === 'Escape' && menuButton.getAttribute('aria-expanded') === 'true') {
    closeMenu();
    menuButton.focus();
  }
});

// A native selector keeps the curated examples easy to browse; without JS all remain readable.
const exampleSelect = document.querySelector('#example-select');
if (exampleSelect) {
  const panels = [...document.querySelectorAll('[data-example]')];
  const announcement = document.querySelector('#announcement');
  const chooseExample = (key, announce = false) => {
    const selected = panels.find(panel => panel.dataset.example === key) || panels[0];
    if (!selected) return;
    for (const panel of panels) panel.hidden = panel !== selected;
    exampleSelect.value = selected.dataset.example;
    if (announce && announcement) announcement.textContent = `${selected.querySelector('h2').textContent} example selected.`;
  };
  exampleSelect.closest('.example-picker').hidden = false;
  chooseExample(location.hash.slice(1));
  exampleSelect.addEventListener('change', () => {
    chooseExample(exampleSelect.value, true);
    history.replaceState(null, '', `#${exampleSelect.value}`);
  });
  window.addEventListener('hashchange', () => chooseExample(location.hash.slice(1)));
}

const guideNavigation = document.querySelector('.guide-sidebar nav');
const currentGuide = guideNavigation?.querySelector('[aria-current="page"]');
if (currentGuide && guideNavigation.scrollHeight > guideNavigation.clientHeight) {
  guideNavigation.scrollTop += currentGuide.getBoundingClientRect().top
    - guideNavigation.getBoundingClientRect().top - guideNavigation.clientHeight / 2;
}
