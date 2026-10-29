#!/usr/bin/env python3
"""Render complete example networks using the production C layout and sprites.

This is a host review tool. Generated images are schematics, not A5000 captures.
Pillow is needed only for this tool, never for the native application.
"""
import argparse
import base64
import html
import io
import json
from pathlib import Path
import subprocess
import tempfile
from sprites import canvas

ROOT = Path(__file__).resolve().parent
EXAMPLES = ['Pipeline', 'Ring', 'Grid 3x3', 'Grid 4x4', 'Binary tree', 'Cube', 'Irregular', 'Parallel links']
MODES = ['Automatic', 'Layered', 'Host distance']


def model(binary, example, mode):
    lines = subprocess.check_output([str(binary), str(example), str(mode)], text=True).splitlines()
    _, w, h, host_y, kind = lines.pop(0).split(' ', 4)
    result = dict(width=int(w), height=int(h), host_y=int(host_y), kind=kind, nodes=[], edges=[])
    for line in lines:
        if line.startswith('N '):
            _, number, x, y, processor, ram = line.split(' ', 5)
            result['nodes'].append(dict(number=int(number), x=int(x), y=int(y), processor=processor, ram=ram))
        else:
            values = list(map(int, line.split()[1:]))
            a, ap, b, bp, count, ax, ay, bx, by = values[:9]
            points = list(zip(values[9::2], values[10::2]))
            assert len(points) == count
            result['edges'].append(dict(a=a, ap=ap, b=b, bp=bp, la=(ax, ay), lb=(bx, by), points=points))
    return result


def metrics(g):
    segments, bends, length = [], 0, 0
    for edge in g['edges']:
        parts = []
        for p, q in zip(edge['points'], edge['points'][1:]):
            if p == q:
                continue
            parts.append((p, q))
            length += abs(p[0]-q[0])+abs(p[1]-q[1])
        bends += max(0, len(parts)-1)
        segments.append(parts)
    crossings = overlaps = 0
    for i, first in enumerate(segments):
        for second in segments[:i]:
            for p, q in first:
                for r, s in second:
                    if p[0] == q[0] and r[0] == s[0] and p[0] == r[0]:
                        overlaps += max(min(p[1], q[1]), min(r[1], s[1])) < min(max(p[1], q[1]), max(r[1], s[1]))
                    elif p[1] == q[1] and r[1] == s[1] and p[1] == r[1]:
                        overlaps += max(min(p[0], q[0]), min(r[0], s[0])) < min(max(p[0], q[0]), max(r[0], s[0]))
                    else:
                        v0, v1, h0, h1 = (p, q, r, s) if p[0] == q[0] else (r, s, p, q)
                        if v0[0] == v1[0] and h0[1] == h1[1]:
                            crossings += min(v0[1], v1[1]) < h0[1] < max(v0[1], v1[1]) and min(h0[0], h1[0]) < v0[0] < max(h0[0], h1[0])
    return dict(crossings=crossings, bends=bends, length=length, overlaps=overlaps, area=g['width']*g['height'])


def chip(host=False):
    from PIL import Image
    pixels, mask = canvas(host)
    result = Image.new('RGBA', (34, 34))
    greys = [255, 221, 187, 153, 119, 85, 51, 0]
    for y in range(34):
        for x in range(34):
            c = greys[pixels[y][x]]
            result.putpixel((x, y), (c, c, c, 255 if mask[y][x] else 0))
    return result


def render(g, title, output):
    from PIL import Image, ImageDraw, ImageFont
    image = Image.new('RGB', ((g['width']+1)//2, (g['height']+1)//2), 'white')
    draw = ImageDraw.Draw(image)
    font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf', 14)
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{g["width"]/2:g}" height="{g["height"]/2:g}" viewBox="0 0 {g["width"]} {g["height"]}">',
           '<rect width="100%" height="100%" fill="white"/>']

    def text(x, y, value, centre=False):
        # Filled native caption icons mask wires beneath the text.
        if centre:
            draw.rectangle(((x-88)/2, (-y-40)/2, (x+88)/2, -y/2), fill='white')
            svg.append(f'<rect x="{x-88}" y="{-y-40}" width="176" height="40" fill="white"/>')
        # y is the native icon's lower boundary. Desktop font is approximated.
        draw.text((x/2, (-y-32)/2), value, fill='black', font=font, anchor='mt' if centre else 'lt')
        anchor = 'middle' if centre else 'start'
        svg.append(f'<text x="{x}" y="{-y-8}" font-family="monospace" font-size="32" text-anchor="{anchor}">{html.escape(value)}</text>')

    text(16, -52, title)
    for edge in g['edges']:
        points = edge['points']
        draw.line([(x/2, -y/2) for x, y in points], fill='#004499', width=1)
        svg.append('<polyline fill="none" stroke="#004499" stroke-width="2" points="'+' '.join(f'{x},{-y}' for x, y in points)+'"/>')
    for edge in g['edges']:
        text(*edge['la'], 'Host' if edge['a'] < 0 else str(edge['ap']))
        text(*edge['lb'], str(edge['bp']))
    icons = [(96, g['host_y'], 'Host', '', True)]
    icons += [(n['x'], n['y'], f'{n["number"]}: {n["processor"]}', n['ram'], False) for n in g['nodes']]
    for x, y, label, ram, host in icons:
        sprite = chip(host)
        image.paste(sprite, ((x-34)//2, (-y-34)//2), sprite)
        buffer = io.BytesIO(); sprite.save(buffer, format='PNG')
        encoded = base64.b64encode(buffer.getvalue()).decode()
        svg.append(f'<image x="{x-34}" y="{-y-34}" width="68" height="68" href="data:image/png;base64,{encoded}"/>')
        text(x, y-92, label, True)
        if ram:
            text(x, y-132, ram, True)
    svg.append('</svg>')
    output.with_suffix('.svg').write_text('\n'.join(svg)+'\n')
    image.save(output.with_suffix('.png'))


def build_previews(destination):
    destination.mkdir(parents=True, exist_ok=True)
    all_metrics, sections = {}, []
    with tempfile.TemporaryDirectory() as work:
        binary = Path(work)/'layout'
        subprocess.run(['cc', '-std=c90', '-pedantic-errors', '-O2', str(ROOT/'test_view.c'),
                        *[str(ROOT/name) for name in ('graph.c', 'quality.c', 'examples.c', 'present.c')], '-o', str(binary)], check=True)
        for example, name in enumerate(EXAMPLES):
            panels = []
            for mode, label in enumerate(MODES):
                graph = model(binary, example, mode)
                values = metrics(graph)
                assert values['overlaps'] == 0, (name, label, values)
                stem = f'{example}-{mode}'
                render(graph, f'{name} / {label} / {graph["kind"]}', destination/stem)
                all_metrics[stem] = dict(example=name, mode=label, layout=graph['kind'], width=graph['width'], height=graph['height'], **values)
                caption = f'{label}: {graph["kind"]}; {values["crossings"]} crossings, {values["bends"]} bends, length {values["length"]} OS units; {graph["width"]} × {graph["height"]}'
                panels.append(f'<article data-mode="{mode}"><h3>{html.escape(caption)}</h3><div class="diagram"><img src="{stem}.svg" alt="{name}, {label}"></div><a href="{stem}.png">PNG</a></article>')
            sections.append(f'<section data-example="{example}"><h2>{name}</h2>'+''.join(panels)+'</section>')
    (destination/'metrics.json').write_text(json.dumps(all_metrics, indent=2)+'\n')
    (destination/'index.html').write_text('''<!doctype html><html lang="en"><meta charset="utf-8"><title>Ispy layout comparison</title>
<style>body{font:16px sans-serif;margin:24px;max-width:1400px}section{border-top:2px solid #888;margin-top:28px}article{border:1px solid #bbb;padding:12px;margin:12px 0}.diagram{overflow:auto;max-height:75vh;background:white}img{display:block}h3{font-size:16px}label{margin-right:24px}select{font:inherit}[hidden]{display:none}</style>
<h1>Ispy layout comparison</h1><p>Complete synthetic networks, computed by the production C code. These host schematics use the application sprites with an approximate desktop font; they are not A5000 screenshots. Port numbers are illustrative. Crossings count proper perpendicular intersections; bends and length describe the actual paths. Every view has zero shared segments. All links, including host and parallel links, are retained.</p><p><label>Example <select id="example"><option value="all">All examples</option>'''+''.join(f'<option value="{i}">{name}</option>' for i,name in enumerate(EXAMPLES))+'''</select></label><label>Layout <select id="mode"><option value="0">Automatic</option><option value="1">Layered</option><option value="2">Host distance</option><option value="all">Compare all three</option></select></label></p>'''+''.join(sections)+'''<script>function filter(){const e=document.getElementById('example').value,m=document.getElementById('mode').value;document.querySelectorAll('section').forEach(s=>s.hidden=e!=='all'&&s.dataset.example!==e);document.querySelectorAll('article').forEach(a=>a.hidden=m!=='all'&&a.dataset.mode!==m)}document.querySelectorAll('select').forEach(s=>s.addEventListener('change',filter));filter()</script></html>\n''')
    return all_metrics


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    results = build_previews(args.destination)
    for key, values in results.items():
        if key.endswith('-0'):
            baseline = results[key[:-1]+'2']
            print(f'{values["example"]}: crossings {baseline["crossings"]} → {values["crossings"]}, bends {baseline["bends"]} → {values["bends"]}, width {baseline["width"]} → {values["width"]}')
