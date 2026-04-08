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
