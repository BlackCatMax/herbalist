import io, re, markdown, html

SRC = 'G:/herbalist/docs/design/ART_BRIEF_Creatures_And_People.md'
OUT = 'G:/herbalist/build/docs_pages/art_brief/index.html'

md = io.open(SRC, encoding='utf-8').read()
# drop the H1 and the repo-path sentence (page has its own header)
md = re.sub(r'^# .*\n', '', md, count=1)
md = md.replace('Источник образов — карточки бестиария\n(`herbalist_docs/Herbalist_Vault/04_Compendium/Бестиарий/`), там же поверья.',
                'Источник образов — карточки бестиария проекта, там же поверья.')

body = markdown.markdown(md, extensions=['tables'])

RANKS = [('Легендарн', 'legend', 'Легендарный'), ('Основной', 'main', 'Основной'),
         ('Низший', 'low', 'Низший')]

def slug(t):
    t = re.sub(r'<[^>]+>', '', t).lower()
    t = re.sub(r'[^\w]+', '-', t, flags=re.U).strip('-')
    return t

# h2 ids + toc, h3 -> article cards (sequential pass)
toc = []
def rank_of(tail):
    for key, cls, label in RANKS:
        if key in tail:
            return cls
    return 'other'
tokens = re.split(r'(<h2>.*?</h2>|<h3>.*?</h3>|<hr />)', body)
out = []
open_article = False
for tok in tokens:
    m2 = re.fullmatch(r'<h2>(.*?)</h2>', tok)
    m3 = re.fullmatch(r'<h3>(.*?)</h3>', tok)
    if m2 or tok == '<hr />':
        if open_article:
            out.append('</article>')
            open_article = False
        if m2:
            t = m2.group(1)
            sid = slug(t)
            toc.append((sid, re.sub(r'<[^>]+>', '', t)))
            out.append(f'<h2 id="{sid}">{t}</h2>')
        else:
            out.append(tok)
    elif m3:
        if open_article:
            out.append('</article>')
        t = m3.group(1)
        mm = re.match(r'(.*?)\s+—\s+(.*)$', t)
        name, chip = t, ''
        if mm:
            name, tail = mm.group(1), mm.group(2)
            chip = f'<span class="chip chip-{rank_of(tail)}">{tail}</span>'
        out.append(f'<article class="card" id="{slug(name)}"><header class="card-h"><h3>{name}</h3>{chip}</header>')
        open_article = True
    else:
        out.append(tok)
if open_article:
    out.append('</article>')
body = ''.join(out)

# "Главная идея" as lead line
body = re.sub(r'<p><strong>Главная идея:</strong>\s*(.*?)</p>', r'<p class="idea"><span class="idea-l">Главная идея</span>\1</p>', body, flags=re.S)
# tables scroll wrappers
body = body.replace('<table>', '<div class="tbl"><table>').replace('</table>', '</table></div>')

# palette swatches for the biome table
SW = {
 'Болото': ['#4a4a2a', '#6b5a3a', '#8a5a2b'],
 'Лесостепь': ['#d8c38a', '#b9a36a', '#8ea0b5'],
 'Речная пойма': ['#5f7d6b', '#9fb3a6', '#c9d0d3'],
 'Смешанный лес': ['#6f8f4e', '#a7b36f', '#e6dfcf'],
 'Степь': ['#c49a4f', '#a37b3c', '#d9c49a'],
 'Тайга': ['#2f4a37', '#4f6b4a', '#7b8a6a'],
 'Тундра': ['#eef1f4', '#8d97a5', '#6b5b8f'],
 'Широколиственный лес': ['#7a5436', '#a4784c', '#c7a26b'],
}
def swatch_row(m):
    name = m.group(1)
    if name not in SW:
        return m.group(0)
    dots = ''.join(f'<i style="background:{c}"></i>' for c in SW[name])
    return f'<td><span class="sw">{dots}</span>{name}</td>'
body = re.sub(r'<td>([^<]+)</td>', swatch_row, body)

nav = '\n'.join(f'<li><a href="#{sid}">{html.escape(t)}</a></li>' for sid, t in toc)

page = f'''<title>Бестиарий для концептера</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Kurale&family=Literata:ital,opsz,wght@0,7..72,400;0,7..72,600;1,7..72,400&family=IBM+Plex+Sans+Condensed:wght@400;600&display=swap">
<style>
:root {{
  --bg: #eef0ea; --paper: #f7f8f4; --ink: #22251f; --muted: #5b6254;
  --line: #d3d8cb; --accent: #8c2f39; --moss: #55663f;
  --low: #55663f; --main: #7a5a1e; --legend: #8c2f39; --other: #4b5a6e;
  --chip-bg: #e3e7dc;
}}
@media (prefers-color-scheme: dark) {{
  :root:not([data-theme="light"]) {{
    color-scheme: dark;
    --bg: #151814; --paper: #1c201b; --ink: #e3e6dc; --muted: #a2a998;
    --line: #333a30; --accent: #e0909a; --moss: #a9bd8c;
    --low: #a9bd8c; --main: #d9b56b; --legend: #e0909a; --other: #9fb3cc;
    --chip-bg: #262b24;
  }}
}}
:root[data-theme="dark"] {{
  color-scheme: dark;
  --bg: #151814; --paper: #1c201b; --ink: #e3e6dc; --muted: #a2a998;
  --line: #333a30; --accent: #e0909a; --moss: #a9bd8c;
  --low: #a9bd8c; --main: #d9b56b; --legend: #e0909a; --other: #9fb3cc;
  --chip-bg: #262b24;
}}
body {{ background: var(--bg); color: var(--ink); font: 16px/1.6 "Literata", Georgia, "Times New Roman", serif; }}
.wrap {{ max-width: 1120px; margin: 0 auto; padding-inline: 20px; padding-block: 40px 80px;
  display: grid; grid-template-columns: 220px minmax(0, 1fr); gap: 48px; }}
header.top {{ grid-column: 1 / -1; border-bottom: 1px solid var(--line); padding-bottom: 24px; }}
.eyebrow {{ font: 600 12px/1 "IBM Plex Sans Condensed", "Arial Narrow", sans-serif; letter-spacing: .12em;
  text-transform: uppercase; color: var(--accent); }}
h1 {{ font: 44px/1.1 "Kurale", Georgia, serif; margin: 10px 0 12px; text-wrap: balance; }}
.lede {{ max-width: 62ch; color: var(--muted); margin: 0; }}
nav.toc {{ position: sticky; top: calc(env(safe-area-inset-top, 0px) + 24px); align-self: start;
  font: 14px/1.4 "IBM Plex Sans Condensed", "Arial Narrow", sans-serif; }}
nav.toc ol {{ list-style: none; margin: 0; padding: 0; display: grid; gap: 2px; }}
nav.toc a {{ display: block; padding: 4px 10px; border-left: 2px solid var(--line); color: var(--muted); text-decoration: none; }}
nav.toc a:hover, nav.toc a:focus-visible {{ color: var(--ink); border-left-color: var(--accent); outline: none; }}
main {{ min-width: 0; }}
main > p, main > ul, main > ol {{ max-width: 68ch; }}
h2 {{ font: 30px/1.2 "Kurale", Georgia, serif; margin: 56px 0 16px; padding-top: 8px; text-wrap: balance;
  scroll-margin-top: 16px; }}
h2:first-child {{ margin-top: 0; }}
hr {{ border: 0; border-top: 1px solid var(--line); margin: 48px 0 0; }}
a {{ color: var(--accent); }}
code {{ font: 14px "IBM Plex Sans Condensed", sans-serif; background: var(--chip-bg); padding: 1px 5px; border-radius: 4px; }}
strong {{ font-weight: 600; }}
.tbl {{ overflow-x: auto; margin: 16px 0 24px; }}
table {{ border-collapse: collapse; width: 100%; font: 15px/1.45 "IBM Plex Sans Condensed", "Arial Narrow", sans-serif; }}
th {{ text-align: left; font-weight: 600; color: var(--muted); font-size: 12px; letter-spacing: .08em; text-transform: uppercase;
  border-bottom: 1px solid var(--ink); padding: 8px 12px 8px 0; }}
td {{ border-bottom: 1px solid var(--line); padding: 10px 12px 10px 0; vertical-align: top; }}
.sw {{ display: inline-flex; gap: 3px; margin-right: 10px; vertical-align: -2px; }}
.sw i {{ width: 12px; height: 12px; border-radius: 50%; box-shadow: inset 0 0 0 1px rgba(0,0,0,.15); }}
article.card {{ background: var(--paper); border: 1px solid var(--line); border-radius: 6px;
  padding: 20px 24px 8px; margin: 16px 0; scroll-margin-top: 16px; }}
.card-h {{ display: flex; flex-wrap: wrap; align-items: baseline; gap: 8px 14px; margin-bottom: 6px; }}
.card-h h3 {{ font: 23px/1.2 "Kurale", Georgia, serif; margin: 0; }}
.chip {{ font: 600 12px/1 "IBM Plex Sans Condensed", sans-serif; letter-spacing: .06em; text-transform: uppercase;
  padding: 5px 8px; border-radius: 3px; background: var(--chip-bg); }}
.chip-low {{ color: var(--low); }} .chip-main {{ color: var(--main); }}
.chip-legend {{ color: var(--legend); box-shadow: inset 0 0 0 1px currentColor; }}
.chip-other {{ color: var(--other); }}
.idea {{ font-style: italic; font-size: 18px; margin: 4px 0 12px; max-width: 60ch; }}
.idea-l {{ display: block; font: 600 11px/1 "IBM Plex Sans Condensed", sans-serif; font-style: normal; letter-spacing: .12em;
  text-transform: uppercase; color: var(--accent); margin-bottom: 6px; }}
article.card ul {{ padding-left: 20px; margin: 0 0 14px; max-width: 66ch; }}
article.card li {{ margin: 3px 0; }}
article.card li:last-child strong, article.card p strong {{ color: var(--ink); }}
@media (max-width: 820px) {{
  .wrap {{ grid-template-columns: minmax(0, 1fr); gap: 24px; padding-inline: 16px; }}
  nav.toc {{ position: static; }}
  nav.toc ol {{ display: flex; flex-wrap: wrap; gap: 6px; }}
  nav.toc a {{ border: 1px solid var(--line); border-radius: 3px; padding: 4px 8px; }}
  h1 {{ font-size: 34px; }}
  article.card {{ padding: 16px 16px 6px; }}
}}
@media (prefers-reduced-motion: no-preference) {{ html {{ scroll-behavior: smooth; }} }}
</style>
<div class="wrap">
  <header class="top">
    <div class="eyebrow">Травник · ТЗ концепт-художнику</div>
    <h1>Существа и люди</h1>
    <p class="lede">Все существа и люди проекта, кроме Заряны и Индрика-зверя: как игрок их видит, главная идея образа, внешность, вид «под Мороком», связь с игрой и что нарисовать.</p>
  </header>
  <nav class="toc" aria-label="Разделы"><ol>
{nav}
  </ol></nav>
  <main>
{body}
  </main>
</div>
'''
import os
os.makedirs(os.path.dirname(OUT), exist_ok=True)
io.open(OUT, 'w', encoding='utf-8', newline='').write(page)
print('ok', len(page), 'articles', page.count('<article'), '/', page.count('</article>'))
