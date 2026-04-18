const { mathjax } = require('mathjax-full/js/mathjax.js');
const { TeX } = require('mathjax-full/js/input/tex.js');
const { SVG } = require('mathjax-full/js/output/svg.js');
const { LiteAdaptor } = require('mathjax-full/js/adaptors/liteAdaptor.js');
const { RegisterHTMLHandler } = require('mathjax-full/js/handlers/html.js');
const { AllPackages } = require('mathjax-full/js/input/tex/AllPackages.js');
const adaptor = new LiteAdaptor();
RegisterHTMLHandler(adaptor);
const mj = mathjax;
const tex = new TeX({ packages: ['base', 'ams'] });
const svgJax = new SVG({ fontCache: 'local' }); 
const html = mj.document('', { InputJax: tex, OutputJax: svgJax });
const input = process.argv[2] || '';
const node = html.convert(input, { display: true });
const fullHTML = adaptor.outerHTML(node);
const styles = adaptor.textContent(svgJax.styleSheet(html));
const svgMatch = fullHTML.match(/<svg[^>]*>([\s\S]*)<\/svg>/);
const svgTagMatch = fullHTML.match(/<svg([^>]*)>/);
if (!svgMatch || !svgTagMatch) {process.exit(1);}
let svgAttributes = svgTagMatch[1];
let svgInnerContent = svgMatch[1];
const vbMatch = svgAttributes.match(/viewBox="([^"]*)"/);
let bgRect = '';
let newViewBox = '';
if (vbMatch) {
    let [vx, vy, vw, vh] = vbMatch[1].split(/\s+/).map(parseFloat);
    const padW = vw * 0.1;
    const padH = vh * 0.1;
    vx -= padW / 2;
    vy -= padH / 2;
    vw += padW;
    vh += padH;
    newViewBox = `${vx} ${vy} ${vw} ${vh}`;
    bgRect = `<rect x="${vx}" y="${vy}" width="${vw}" height="${vh}" fill="black" />`;
}
let finalAttributes = `viewBox="${newViewBox}" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink"`;
const finalStyles = styles + `
    svg { color: white; fill: white; stroke: white; }
    path { fill: white; stroke: none; }
`;
const finalSvg = `<?xml version="1.0" encoding="UTF-8"?>
<svg ${finalAttributes}>
    <style type="text/css">
        ${finalStyles}
    </style>
    ${bgRect}
    ${svgInnerContent}
</svg>`.trim();
console.log(finalSvg);