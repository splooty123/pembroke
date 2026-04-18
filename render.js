const mj = require('mathjax-full/js/mathjax.js').mathjax;
const TeX = require('mathjax-full/js/input/tex.js').TeX;
const SVG = require('mathjax-full/js/output/svg.js').SVG;
const liteAdaptor = require('mathjax-full/js/adaptors/liteAdaptor.js').liteAdaptor;
const RegisterHTMLHandler = require('mathjax-full/js/handlers/html.js').RegisterHTMLHandler;

const adaptor = liteAdaptor();
RegisterHTMLHandler(adaptor);

const tex = new TeX({packages: ['base', 'ams']});
const svg = new SVG({fontCache: 'none'});

const html = mj.document('', {InputJax: tex, OutputJax: svg});

const input = process.argv[2] || '';
const node = html.convert(input, {display: true});

console.log(adaptor.outerHTML(node));
// 1. Setup MathJax
const tex = new TeX({ packages: ['base', 'ams'] });
const svgJax = new SVG({ fontCache: 'local' }); 
const html = mj.document('', { InputJax: tex, OutputJax: svgJax });

// 2. Process Input
const input = process.argv[2] || '';
const node = html.convert(input, { display: true });

const fullHTML = adaptor.outerHTML(node);
const styles = adaptor.textContent(svgJax.styleSheet(html));

// 3. Extract SVG via Regex
const svgMatch = fullHTML.match(/<svg[^>]*>([\s\S]*)<\/svg>/);
const svgTagMatch = fullHTML.match(/<svg([^>]*)>/);

if (!svgMatch || !svgTagMatch) {
    process.exit(1);
}

let svgAttributes = svgTagMatch[1];
let svgInnerContent = svgMatch[1];

// 4. Handle ViewBox and Padding
const vbMatch = svgAttributes.match(/viewBox="([^"]*)"/);
let bgRect = '';
let newViewBox = '';

if (vbMatch) {
    let [vx, vy, vw, vh] = vbMatch[1].split(/\s+/).map(parseFloat);
    
    // Add 10% padding to prevent clipping of slanted/italic characters
    const padW = vw * 0.1;
    const padH = vh * 0.1;
    vx -= padW / 2;
    vy -= padH / 2;
    vw += padW;
    vh += padH;
    
    newViewBox = `${vx} ${vy} ${vw} ${vh}`;
    
    // Create BLACK background (C blitter ignores black)
    bgRect = `<rect x="${vx}" y="${vy}" width="${vw}" height="${vh}" fill="black" />`;
}

// 5. THE FIX: Remove width/height attributes completely.
// This prevents the text from "wrapping" or collapsing on itself.
// We only keep the viewBox and namespaces.
let finalAttributes = `viewBox="${newViewBox}" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink"`;

// 6. Force White-on-Black Styles
const finalStyles = styles + `
    svg { color: white; fill: white; stroke: white; }
    path { fill: white; stroke: none; }
`;

// 7. Final Assembly
const finalSvg = `<?xml version="1.0" encoding="UTF-8"?>
<svg ${finalAttributes}>
    <style type="text/css">
        ${finalStyles}
    </style>
    ${bgRect}
    ${svgInnerContent}
</svg>`.trim();

console.log(finalSvg);
