function _typeof(o) { "@babel/helpers - typeof"; return _typeof = "function" == typeof Symbol && "symbol" == typeof Symbol.iterator ? function (o) { return typeof o; } : function (o) { return o && "function" == typeof Symbol && o.constructor === Symbol && o !== Symbol.prototype ? "symbol" : typeof o; }, _typeof(o); }
function _toConsumableArray(r) { return _arrayWithoutHoles(r) || _iterableToArray(r) || _unsupportedIterableToArray(r) || _nonIterableSpread(); }
function _nonIterableSpread() { throw new TypeError("Invalid attempt to spread non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method."); }
function _iterableToArray(r) { if ("undefined" != typeof Symbol && null != r[Symbol.iterator] || null != r["@@iterator"]) return Array.from(r); }
function _arrayWithoutHoles(r) { if (Array.isArray(r)) return _arrayLikeToArray(r); }
function ownKeys(e, r) { var t = Object.keys(e); if (Object.getOwnPropertySymbols) { var o = Object.getOwnPropertySymbols(e); r && (o = o.filter(function (r) { return Object.getOwnPropertyDescriptor(e, r).enumerable; })), t.push.apply(t, o); } return t; }
function _objectSpread(e) { for (var r = 1; r < arguments.length; r++) { var t = null != arguments[r] ? arguments[r] : {}; r % 2 ? ownKeys(Object(t), !0).forEach(function (r) { _defineProperty(e, r, t[r]); }) : Object.getOwnPropertyDescriptors ? Object.defineProperties(e, Object.getOwnPropertyDescriptors(t)) : ownKeys(Object(t)).forEach(function (r) { Object.defineProperty(e, r, Object.getOwnPropertyDescriptor(t, r)); }); } return e; }
function _defineProperty(e, r, t) { return (r = _toPropertyKey(r)) in e ? Object.defineProperty(e, r, { value: t, enumerable: !0, configurable: !0, writable: !0 }) : e[r] = t, e; }
function _toPropertyKey(t) { var i = _toPrimitive(t, "string"); return "symbol" == _typeof(i) ? i : i + ""; }
function _toPrimitive(t, r) { if ("object" != _typeof(t) || !t) return t; var e = t[Symbol.toPrimitive]; if (void 0 !== e) { var i = e.call(t, r || "default"); if ("object" != _typeof(i)) return i; throw new TypeError("@@toPrimitive must return a primitive value."); } return ("string" === r ? String : Number)(t); }
function _slicedToArray(r, e) { return _arrayWithHoles(r) || _iterableToArrayLimit(r, e) || _unsupportedIterableToArray(r, e) || _nonIterableRest(); }
function _nonIterableRest() { throw new TypeError("Invalid attempt to destructure non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method."); }
function _iterableToArrayLimit(r, l) { var t = null == r ? null : "undefined" != typeof Symbol && r[Symbol.iterator] || r["@@iterator"]; if (null != t) { var e, n, i, u, a = [], f = !0, o = !1; try { if (i = (t = t.call(r)).next, 0 === l) { if (Object(t) !== t) return; f = !1; } else for (; !(f = (e = i.call(t)).done) && (a.push(e.value), a.length !== l); f = !0); } catch (r) { o = !0, n = r; } finally { try { if (!f && null != t.return && (u = t.return(), Object(u) !== u)) return; } finally { if (o) throw n; } } return a; } }
function _arrayWithHoles(r) { if (Array.isArray(r)) return r; }
function _createForOfIteratorHelper(r, e) { var t = "undefined" != typeof Symbol && r[Symbol.iterator] || r["@@iterator"]; if (!t) { if (Array.isArray(r) || (t = _unsupportedIterableToArray(r)) || e && r && "number" == typeof r.length) { t && (r = t); var _n = 0, F = function F() {}; return { s: F, n: function n() { return _n >= r.length ? { done: !0 } : { done: !1, value: r[_n++] }; }, e: function e(r) { throw r; }, f: F }; } throw new TypeError("Invalid attempt to iterate non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method."); } var o, a = !0, u = !1; return { s: function s() { t = t.call(r); }, n: function n() { var r = t.next(); return a = r.done, r; }, e: function e(r) { u = !0, o = r; }, f: function f() { try { a || null == t.return || t.return(); } finally { if (u) throw o; } } }; }
function _unsupportedIterableToArray(r, a) { if (r) { if ("string" == typeof r) return _arrayLikeToArray(r, a); var t = {}.toString.call(r).slice(8, -1); return "Object" === t && r.constructor && (t = r.constructor.name), "Map" === t || "Set" === t ? Array.from(r) : "Arguments" === t || /^(?:Ui|I)nt(?:8|16|32)(?:Clamped)?Array$/.test(t) ? _arrayLikeToArray(r, a) : void 0; } }
function _arrayLikeToArray(r, a) { (null == a || a > r.length) && (a = r.length); for (var e = 0, n = Array(a); e < a; e++) n[e] = r[e]; return n; }
if (!Array.prototype.flatMap) Array.prototype.flatMap = function (f) {
  return [].concat.apply([], this.map(f));
};
var PromptLogic = function () {
  var OPENERS = {
    '(': ')',
    '[': ']',
    '{': '}'
  };
  var CLOSERS = {
    ')': '(',
    ']': '[',
    '}': '{'
  };
  var KEYWORDS = ['BREAK', 'AND'];
  var HARD_SEPARATORS = new Set([',', ';', '\n', '\r', '，', '；', '、', '。']);
  var SOFT_SEPARATORS = new Set(['.', '!', '?']);
  var ANGLE_TYPES_RE = /^<([A-Za-z][\w-]*):/;
  var LORA_TYPES_RE = /^(lora|lyco|lycoris|locon|loha|lokr|hypernet)$/i;
  var NUMBER_RE = /^[-+]?(?:\d+(?:\.\d*)?|\.\d+)$/;
  var WORD_CHAR_RE = /(?:[0-9A-Z_a-z\xAA\xB2\xB3\xB5\xB9\xBA\xBC-\xBE\xC0-\xD6\xD8-\xF6\xF8-\u02C1\u02C6-\u02D1\u02E0-\u02E4\u02EC\u02EE\u0370-\u0374\u0376\u0377\u037A-\u037D\u037F\u0386\u0388-\u038A\u038C\u038E-\u03A1\u03A3-\u03F5\u03F7-\u0481\u048A-\u052F\u0531-\u0556\u0558\u0559\u0560-\u0588\u058B\u058C\u05D0-\u05EA\u05EF-\u05F2\u0620-\u064A\u0660-\u0669\u066E\u066F\u0671-\u06D3\u06D5\u06E5\u06E6\u06EE-\u06FC\u06FF\u0710\u0712-\u072F\u074D-\u07A5\u07B1\u07C0-\u07EA\u07F4\u07F5\u07FA\u0800-\u0815\u081A\u0824\u0828\u0840-\u0858\u0860-\u086A\u0870-\u0887\u0889-\u088F\u08A0-\u08C9\u0904-\u0939\u093D\u0950\u0958-\u0961\u0966-\u096F\u0971-\u0980\u0985-\u098C\u098F\u0990\u0993-\u09A8\u09AA-\u09B0\u09B2\u09B6-\u09B9\u09BD\u09CE\u09DC\u09DD\u09DF-\u09E1\u09E6-\u09F1\u09F4-\u09F9\u09FC\u0A05-\u0A0A\u0A0F\u0A10\u0A13-\u0A28\u0A2A-\u0A30\u0A32\u0A33\u0A35\u0A36\u0A38\u0A39\u0A59-\u0A5C\u0A5E\u0A66-\u0A6F\u0A72-\u0A74\u0A85-\u0A8D\u0A8F-\u0A91\u0A93-\u0AA8\u0AAA-\u0AB0\u0AB2\u0AB3\u0AB5-\u0AB9\u0ABD\u0AD0\u0AE0\u0AE1\u0AE6-\u0AEF\u0AF9\u0B05-\u0B0C\u0B0F\u0B10\u0B13-\u0B28\u0B2A-\u0B30\u0B32\u0B33\u0B35-\u0B39\u0B3D\u0B5C\u0B5D\u0B5F-\u0B61\u0B66-\u0B6F\u0B71-\u0B77\u0B83\u0B85-\u0B8A\u0B8E-\u0B90\u0B92-\u0B95\u0B99\u0B9A\u0B9C\u0B9E\u0B9F\u0BA3\u0BA4\u0BA8-\u0BAA\u0BAE-\u0BB9\u0BD0\u0BE6-\u0BF2\u0C05-\u0C0C\u0C0E-\u0C10\u0C12-\u0C28\u0C2A-\u0C39\u0C3D\u0C58-\u0C5A\u0C5C\u0C5D\u0C60\u0C61\u0C66-\u0C6F\u0C78-\u0C7E\u0C80\u0C85-\u0C8C\u0C8E-\u0C90\u0C92-\u0CA8\u0CAA-\u0CB3\u0CB5-\u0CB9\u0CBD\u0CDC-\u0CDE\u0CE0\u0CE1\u0CE6-\u0CEF\u0CF1\u0CF2\u0D04-\u0D0C\u0D0E-\u0D10\u0D12-\u0D3A\u0D3D\u0D4E\u0D54-\u0D56\u0D58-\u0D61\u0D66-\u0D78\u0D7A-\u0D7F\u0D85-\u0D96\u0D9A-\u0DB1\u0DB3-\u0DBB\u0DBD\u0DC0-\u0DC6\u0DE6-\u0DEF\u0E01-\u0E30\u0E32\u0E33\u0E40-\u0E46\u0E50-\u0E59\u0E81\u0E82\u0E84\u0E86-\u0E8A\u0E8C-\u0EA3\u0EA5\u0EA7-\u0EB0\u0EB2\u0EB3\u0EBD\u0EC0-\u0EC4\u0EC6\u0ED0-\u0ED9\u0EDC-\u0EDF\u0F00\u0F20-\u0F33\u0F40-\u0F47\u0F49-\u0F6C\u0F88-\u0F8C\u1000-\u102A\u103F-\u1049\u1050-\u1055\u105A-\u105D\u1061\u1065\u1066\u106E-\u1070\u1075-\u1081\u108E\u1090-\u1099\u10A0-\u10C5\u10C7\u10CD\u10D0-\u10FA\u10FC-\u1248\u124A-\u124D\u1250-\u1256\u1258\u125A-\u125D\u1260-\u1288\u128A-\u128D\u1290-\u12B0\u12B2-\u12B5\u12B8-\u12BE\u12C0\u12C2-\u12C5\u12C8-\u12D6\u12D8-\u1310\u1312-\u1315\u1318-\u135A\u1369-\u137C\u1380-\u138F\u13A0-\u13F5\u13F8-\u13FD\u1401-\u166C\u166F-\u167F\u1681-\u169A\u16A0-\u16EA\u16EE-\u16F8\u1700-\u1711\u171F-\u1731\u1740-\u1751\u1760-\u176C\u176E-\u1770\u1780-\u17B3\u17D7\u17DC\u17E0-\u17E9\u17F0-\u17F9\u1810-\u1819\u1820-\u1878\u1880-\u1884\u1887-\u18A8\u18AA\u18B0-\u18F5\u1900-\u191E\u1946-\u196D\u1970-\u1974\u1980-\u19AB\u19B0-\u19C9\u19D0-\u19DA\u1A00-\u1A16\u1A20-\u1A54\u1A80-\u1A89\u1A90-\u1A99\u1AA7\u1B05-\u1B33\u1B45-\u1B4C\u1B50-\u1B59\u1B83-\u1BA0\u1BAE-\u1BE5\u1C00-\u1C23\u1C40-\u1C49\u1C4D-\u1C7D\u1C80-\u1C8A\u1C90-\u1CBA\u1CBD-\u1CBF\u1CE9-\u1CEC\u1CEE-\u1CF3\u1CF5\u1CF6\u1CFA\u1D00-\u1DBF\u1E00-\u1F15\u1F18-\u1F1D\u1F20-\u1F45\u1F48-\u1F4D\u1F50-\u1F57\u1F59\u1F5B\u1F5D\u1F5F-\u1F7D\u1F80-\u1FB4\u1FB6-\u1FBC\u1FBE\u1FC2-\u1FC4\u1FC6-\u1FCC\u1FD0-\u1FD3\u1FD6-\u1FDB\u1FE0-\u1FEC\u1FF2-\u1FF4\u1FF6-\u1FFC\u2070\u2071\u2074-\u2079\u207F-\u2089\u208F-\u209F\u2102\u2107\u210A-\u2113\u2115\u2119-\u211D\u2124\u2126\u2128\u212A-\u212D\u212F-\u2139\u213C-\u213F\u2145-\u2149\u214E\u2150-\u2189\u2460-\u249B\u24EA-\u24FF\u2776-\u2793\u2C00-\u2CE4\u2CEB-\u2CEE\u2CF2\u2CF3\u2CFD\u2D00-\u2D25\u2D27\u2D2D\u2D30-\u2D67\u2D6F\u2D80-\u2D96\u2DA0-\u2DA6\u2DA8-\u2DAE\u2DB0-\u2DB6\u2DB8-\u2DBE\u2DC0-\u2DC6\u2DC8-\u2DCE\u2DD0-\u2DD6\u2DD8-\u2DDE\u2E2F\u3005-\u3007\u3021-\u3029\u3031-\u3035\u3038-\u303C\u3041-\u3096\u309D-\u309F\u30A1-\u30FA\u30FC-\u30FF\u3105-\u312F\u3131-\u318E\u3192-\u3195\u31A0-\u31BF\u31F0-\u31FF\u3220-\u3229\u3248-\u324F\u3251-\u325F\u3280-\u3289\u32B1-\u32BF\u3400-\u4DBF\u4E00-\uA48C\uA4D0-\uA4FD\uA500-\uA60C\uA610-\uA62B\uA640-\uA66E\uA67F-\uA69D\uA6A0-\uA6EF\uA717-\uA71F\uA722-\uA788\uA78B-\uA7DD\uA7E2\uA7F1-\uA801\uA803-\uA805\uA807-\uA80A\uA80C-\uA822\uA830-\uA835\uA840-\uA873\uA882-\uA8B3\uA8D0-\uA8D9\uA8F2-\uA8F7\uA8FB\uA8FD\uA8FE\uA900-\uA925\uA930-\uA946\uA960-\uA97C\uA984-\uA9B2\uA9CF-\uA9D9\uA9E0-\uA9E4\uA9E6-\uA9FE\uAA00-\uAA28\uAA40-\uAA42\uAA44-\uAA4B\uAA50-\uAA59\uAA60-\uAA76\uAA7A\uAA7E-\uAAAF\uAAB1\uAAB5\uAAB6\uAAB9-\uAABD\uAAC0\uAAC2\uAADB-\uAADD\uAAE0-\uAAEA\uAAF2-\uAAF4\uAB01-\uAB06\uAB09-\uAB0E\uAB11-\uAB16\uAB20-\uAB26\uAB28-\uAB2E\uAB30-\uAB5A\uAB5C-\uAB69\uAB6C\uAB6D\uAB70-\uABE2\uABF0-\uABF9\uAC00-\uD7A3\uD7B0-\uD7C6\uD7CB-\uD7FB\uF900-\uFA6D\uFA70-\uFAD9\uFB00-\uFB06\uFB13-\uFB17\uFB1D\uFB1F-\uFB28\uFB2A-\uFB36\uFB38-\uFB3C\uFB3E\uFB40\uFB41\uFB43\uFB44\uFB46-\uFBB1\uFBD3-\uFD3D\uFD50-\uFD8F\uFD92-\uFDC7\uFDF0-\uFDFB\uFE70-\uFE74\uFE76-\uFEFC\uFF10-\uFF19\uFF21-\uFF3A\uFF41-\uFF5A\uFF66-\uFFBE\uFFC2-\uFFC7\uFFCA-\uFFCF\uFFD2-\uFFD7\uFFDA-\uFFDC]|\uD800[\uDC00-\uDC0B\uDC0D-\uDC26\uDC28-\uDC3A\uDC3C\uDC3D\uDC3F-\uDC4D\uDC50-\uDC5D\uDC80-\uDCFA\uDD07-\uDD33\uDD40-\uDD78\uDD8A\uDD8B\uDE80-\uDE9C\uDEA0-\uDED0\uDEE1-\uDEFB\uDF00-\uDF23\uDF2D-\uDF4A\uDF50-\uDF75\uDF80-\uDF9D\uDFA0-\uDFC3\uDFC8-\uDFCF\uDFD1-\uDFD5]|\uD801[\uDC00-\uDC9D\uDCA0-\uDCA9\uDCB0-\uDCD3\uDCD8-\uDCFB\uDD00-\uDD27\uDD30-\uDD63\uDD70-\uDD7A\uDD7C-\uDD8A\uDD8C-\uDD92\uDD94\uDD95\uDD97-\uDDA1\uDDA3-\uDDB1\uDDB3-\uDDB9\uDDBB\uDDBC\uDDC0-\uDDF3\uDE00-\uDF36\uDF40-\uDF55\uDF60-\uDF67\uDF80-\uDF85\uDF87-\uDFB0\uDFB2-\uDFBF]|\uD802[\uDC00-\uDC05\uDC08\uDC0A-\uDC35\uDC37\uDC38\uDC3C\uDC3F-\uDC55\uDC58-\uDC76\uDC79-\uDC9E\uDCA7-\uDCAF\uDCE0-\uDCF2\uDCF4\uDCF5\uDCFB-\uDD1B\uDD20-\uDD39\uDD40-\uDD59\uDD80-\uDDB7\uDDBC-\uDDCF\uDDD2-\uDE00\uDE10-\uDE13\uDE15-\uDE17\uDE19-\uDE35\uDE40-\uDE48\uDE60-\uDE7E\uDE80-\uDE9F\uDEC0-\uDEC7\uDEC9-\uDEE4\uDEEB-\uDEEF\uDF00-\uDF35\uDF40-\uDF55\uDF58-\uDF72\uDF78-\uDF91\uDFA9-\uDFAF]|\uD803[\uDC00-\uDC48\uDC80-\uDCB2\uDCC0-\uDCF2\uDCFA-\uDD23\uDD30-\uDD39\uDD40-\uDD65\uDD6F-\uDD85\uDE60-\uDE7E\uDE80-\uDEA9\uDEB0\uDEB1\uDEC2-\uDEC7\uDED9-\uDEEE\uDF00-\uDF27\uDF30-\uDF45\uDF51-\uDF54\uDF70-\uDF81\uDFB0-\uDFCB\uDFE0-\uDFF6]|\uD804[\uDC03-\uDC37\uDC52-\uDC6F\uDC71\uDC72\uDC75\uDC83-\uDCAF\uDCD0-\uDCE8\uDCF0-\uDCF9\uDD03-\uDD26\uDD36-\uDD3F\uDD44\uDD47\uDD50-\uDD72\uDD76\uDD83-\uDDB2\uDDC1-\uDDC4\uDDD0-\uDDDA\uDDDC\uDDE1-\uDDF4\uDE00-\uDE11\uDE13-\uDE2B\uDE3F\uDE40\uDE80-\uDE86\uDE88\uDE8A-\uDE8D\uDE8F-\uDE9D\uDE9F-\uDEA8\uDEB0-\uDEDE\uDEF0-\uDEF9\uDF05-\uDF0C\uDF0F\uDF10\uDF13-\uDF28\uDF2A-\uDF30\uDF32\uDF33\uDF35-\uDF39\uDF3D\uDF50\uDF5D-\uDF61\uDF80-\uDF89\uDF8B\uDF8E\uDF90-\uDFB5\uDFB7\uDFD1\uDFD3]|\uD805[\uDC00-\uDC34\uDC47-\uDC4A\uDC50-\uDC59\uDC5F-\uDC61\uDC80-\uDCAF\uDCC4\uDCC5\uDCC7\uDCD0-\uDCD9\uDD80-\uDDAE\uDDD8-\uDDDB\uDE00-\uDE2F\uDE44\uDE50-\uDE59\uDE80-\uDEAA\uDEB8\uDEC0-\uDEC9\uDED0-\uDEE3\uDF00-\uDF1A\uDF30-\uDF3B\uDF40-\uDF46]|\uD806[\uDC00-\uDC2B\uDCA0-\uDCF2\uDCFF-\uDD06\uDD09\uDD0C-\uDD13\uDD15\uDD16\uDD18-\uDD2F\uDD3F\uDD41\uDD50-\uDD59\uDDA0-\uDDA7\uDDAA-\uDDD0\uDDE1\uDDE3\uDE00\uDE0B-\uDE32\uDE3A\uDE50\uDE5C-\uDE89\uDE9D\uDEB0-\uDEF8\uDF0A\uDFC0-\uDFE0\uDFF0-\uDFF9]|\uD807[\uDC00-\uDC08\uDC0A-\uDC2E\uDC40\uDC50-\uDC6C\uDC72-\uDC8F\uDD00-\uDD06\uDD08\uDD09\uDD0B-\uDD30\uDD46\uDD50-\uDD59\uDD60-\uDD65\uDD67\uDD68\uDD6A-\uDD89\uDD98\uDDA0-\uDDA9\uDDB0-\uDDDB\uDDE0-\uDDE9\uDDF1\uDEE0-\uDEF2\uDF02\uDF04-\uDF10\uDF12-\uDF33\uDF50-\uDF59\uDFB0\uDFC0-\uDFD4]|\uD808[\uDC00-\uDF99]|\uD809[\uDC00-\uDC6F\uDC75-\uDD43\uDD50-\uDE86]|\uD80B[\uDF90-\uDFF0]|[\uD80C\uD80E\uD80F\uD81C-\uD822\uD840-\uD868\uD86A-\uD86D\uD86F-\uD872\uD874-\uD879\uD880-\uD883\uD885-\uD88C\uD8B4-\uD8BE][\uDC00-\uDFFF]|\uD80D[\uDC00-\uDC2F\uDC41-\uDC46\uDC60-\uDFFF]|\uD810[\uDC00-\uDFFA]|\uD811[\uDC00-\uDE46]|\uD818[\uDD00-\uDD1D\uDD30-\uDD39]|\uD81A[\uDC00-\uDE38\uDE40-\uDE5E\uDE60-\uDE69\uDE70-\uDEBE\uDEC0-\uDEC9\uDED0-\uDEED\uDF00-\uDF2F\uDF40-\uDF43\uDF50-\uDF59\uDF5B-\uDF61\uDF63-\uDF77\uDF7D-\uDF8F]|\uD81B[\uDD40-\uDD6C\uDD70-\uDD79\uDE40-\uDE96\uDEA0-\uDEB8\uDEBB-\uDED3\uDF00-\uDF4A\uDF50\uDF93-\uDF9F\uDFE0\uDFE1\uDFE3\uDFF2-\uDFF6]|\uD823[\uDC00-\uDCDA\uDCFF-\uDD20\uDD80-\uDDF2\uDE00-\uDFFF]|\uD824[\uDC00-\uDD91\uDDA0-\uDDD2]|\uD82B[\uDFF0-\uDFF3\uDFF5-\uDFFB\uDFFD\uDFFE]|\uD82C[\uDC00-\uDD28\uDD32\uDD50-\uDD52\uDD55\uDD64-\uDD68\uDD70-\uDEFB]|\uD82F[\uDC00-\uDC6A\uDC70-\uDC7C\uDC80-\uDC88\uDC90-\uDC99]|\uD833[\uDCF0-\uDCF9]|\uD834[\uDEC0-\uDED3\uDEE0-\uDEF3\uDF60-\uDF78]|\uD835[\uDC00-\uDC54\uDC56-\uDC9C\uDC9E\uDC9F\uDCA2\uDCA5\uDCA6\uDCA9-\uDCAC\uDCAE-\uDCB9\uDCBB\uDCBD-\uDCC3\uDCC5-\uDD05\uDD07-\uDD0A\uDD0D-\uDD14\uDD16-\uDD1C\uDD1E-\uDD39\uDD3B-\uDD3E\uDD40-\uDD44\uDD46\uDD4A-\uDD50\uDD52-\uDEA6\uDEA8-\uDEC0\uDEC2-\uDEDA\uDEDC-\uDEFA\uDEFC-\uDF14\uDF16-\uDF34\uDF36-\uDF4E\uDF50-\uDF6E\uDF70-\uDF88\uDF8A-\uDFA8\uDFAA-\uDFC2\uDFC4-\uDFCB\uDFCE-\uDFFF]|\uD837[\uDF00-\uDF81\uDF90-\uDF96\uDFCD-\uDFFF]|\uD838[\uDC30-\uDC6D\uDD00-\uDD2C\uDD37-\uDD3D\uDD40-\uDD49\uDD4E\uDE90-\uDEAD\uDEC0-\uDEEB\uDEF0-\uDEF9]|\uD839[\uDCD0-\uDCEB\uDCF0-\uDCF9\uDDD0-\uDDED\uDDF0-\uDDFA\uDEC0-\uDEDE\uDEE0-\uDEE2\uDEE4\uDEE5\uDEE7-\uDEED\uDEF0-\uDEF4\uDEFE\uDEFF\uDFE0-\uDFE6\uDFE8-\uDFEB\uDFED\uDFEE\uDFF0-\uDFFE]|\uD83A[\uDC00-\uDCC4\uDCC7-\uDCCF\uDD00-\uDD43\uDD4B\uDD50-\uDD59]|\uD83B[\uDC71-\uDCAB\uDCAD-\uDCAF\uDCB1-\uDCB4\uDD01-\uDD2D\uDD2F-\uDD3D\uDE00-\uDE03\uDE05-\uDE1F\uDE21\uDE22\uDE24\uDE27\uDE29-\uDE32\uDE34-\uDE37\uDE39\uDE3B\uDE42\uDE47\uDE49\uDE4B\uDE4D-\uDE4F\uDE51\uDE52\uDE54\uDE57\uDE59\uDE5B\uDE5D\uDE5F\uDE61\uDE62\uDE64\uDE67-\uDE6A\uDE6C-\uDE72\uDE74-\uDE77\uDE79-\uDE7C\uDE7E\uDE80-\uDE89\uDE8B-\uDE9B\uDEA1-\uDEA3\uDEA5-\uDEA9\uDEAB-\uDEBB]|\uD83C[\uDD00-\uDD0C]|\uD83E[\uDFF0-\uDFF9]|\uD869[\uDC00-\uDEDF\uDF00-\uDFFF]|\uD86E[\uDC00-\uDC1E\uDC20-\uDFFF]|\uD873[\uDC00-\uDEAD\uDEB0-\uDFFF]|\uD87A[\uDC00-\uDFE0\uDFF0-\uDFFF]|\uD87B[\uDC00-\uDE5D]|\uD87E[\uDC00-\uDE1D]|\uD884[\uDC00-\uDF4A\uDF50-\uDFFF]|\uD88D[\uDC00-\uDC79]|\uD8BF[\uDC00-\uDC3F])/;
  var WEIGHT_MIN = 0;
  var WEIGHT_MAX = 2;
  var LORA_MIN = -2;
  var LORA_MAX = 2;
  var WEIGHT_STEP = 0.1;
  var isSpace = function isSpace(ch) {
    return ch === ' ' || ch === '\t' || ch === ' ' || ch === '　';
  };
  var isWordChar = function isWordChar(ch) {
    return !!ch && WORD_CHAR_RE.test(ch);
  };
  function scanBrackets(text) {
    var n = text.length;
    var match = new Int32Array(n).fill(-1);
    var angleEnd = new Int32Array(n).fill(-1);
    var unmatched = [];
    var brokenAngles = [];
    var stack = [];
    for (var i = 0; i < n; i++) {
      var ch = text[i];
      if (ch === '\\') {
        i++;
        continue;
      }
      if (ch === '<') {
        var head = ANGLE_TYPES_RE.exec(text.slice(i, i + 40));
        if (head) {
          var j = i + 1;
          while (j < n && text[j] !== '>' && text[j] !== '<' && text[j] !== '\n') j++;
          if (j < n && text[j] === '>') {
            angleEnd[i] = j;
            i = j;
            continue;
          }
          if (LORA_TYPES_RE.test(head[1])) brokenAngles.push(i);
        } else if (/^<(lora|lyco|lycoris|hypernet)\b/i.test(text.slice(i, i + 12))) {
          brokenAngles.push(i);
        }
        continue;
      }
      if (OPENERS[ch]) {
        stack.push(i);
        continue;
      }
      if (CLOSERS[ch]) {
        var top = stack.length ? stack[stack.length - 1] : -1;
        if (top >= 0 && text[top] === CLOSERS[ch]) {
          stack.pop();
          match[top] = i;
          match[i] = top;
        } else {
          unmatched.push(i);
        }
      }
    }
    for (var _i = 0, _stack = stack; _i < _stack.length; _i++) {
      var _i2 = _stack[_i];
      unmatched.push(_i2);
    }
    unmatched.sort(function (a, b) {
      return a - b;
    });
    return {
      match: match,
      angleEnd: angleEnd,
      unmatched: unmatched,
      brokenAngles: brokenAngles
    };
  }
  function keywordAt(text, i) {
    var prev = i > 0 ? text[i - 1] : '';
    if (isWordChar(prev) || prev === '\\') return null;
    var _iterator = _createForOfIteratorHelper(KEYWORDS),
      _step;
    try {
      for (_iterator.s(); !(_step = _iterator.n()).done;) {
        var kw = _step.value;
        if (text.startsWith(kw, i) && !isWordChar(text[i + kw.length] || '')) {
          return kw;
        }
      }
    } catch (err) {
      _iterator.e(err);
    } finally {
      _iterator.f();
    }
    return null;
  }
  function segmentPrompt(text) {
    var src = typeof text === 'string' ? text : '';
    var n = src.length;
    var _scanBrackets = scanBrackets(src),
      match = _scanBrackets.match,
      angleEnd = _scanBrackets.angleEnd;
    var raw = [];
    var segStart = -1;
    var lastNonSpace = -1;
    var flush = function flush(endExclusive) {
      if (segStart < 0) return;
      var end = Math.min(endExclusive, lastNonSpace + 1);
      if (end > segStart) raw.push([segStart, end]);
      segStart = -1;
    };
    var begin = function begin(i) {
      if (segStart < 0) segStart = i;
    };
    var i = 0;
    while (i < n) {
      var ch = src[i];
      if (ch === '\\') {
        begin(i);
        lastNonSpace = Math.min(i + 1, n - 1);
        i += 2;
        continue;
      }
      if (OPENERS[ch] && match[i] > i) {
        begin(i);
        lastNonSpace = match[i];
        i = match[i] + 1;
        continue;
      }
      if (ch === '<' && angleEnd[i] > i) {
        flush(i);
        raw.push([i, angleEnd[i] + 1]);
        i = angleEnd[i] + 1;
        continue;
      }
      if (HARD_SEPARATORS.has(ch)) {
        flush(i);
        i++;
        continue;
      }
      if (SOFT_SEPARATORS.has(ch)) {
        var next = src[i + 1];
        if (next === undefined || isSpace(next) || next === '\n' || next === '\r') {
          flush(i);
          i++;
          continue;
        }
      }
      if (isSpace(ch)) {
        i++;
        continue;
      }
      if (ch === 'B' || ch === 'A') {
        var kw = keywordAt(src, i);
        if (kw) {
          flush(i);
          raw.push([i, i + kw.length]);
          i += kw.length;
          continue;
        }
      }
      begin(i);
      lastNonSpace = i;
      i++;
    }
    flush(n);
    return raw.map(function (_ref, index) {
      var _ref2 = _slicedToArray(_ref, 2),
        start = _ref2[0],
        end = _ref2[1];
      return _objectSpread({
        index: index,
        start: start,
        end: end
      }, classifySegment(src.slice(start, end)));
    });
  }
  function segmentAt(segments, pos) {
    var lo = 0;
    var hi = segments.length - 1;
    while (lo <= hi) {
      var mid = lo + hi >> 1;
      var s = segments[mid];
      if (pos < s.start) hi = mid - 1;else if (pos > s.end) lo = mid + 1;else return mid;
    }
    return -1;
  }
  function splitExplicitWeight(inner) {
    var _scanBrackets2 = scanBrackets(inner),
      match = _scanBrackets2.match;
    var colon = -1;
    for (var i = 0; i < inner.length; i++) {
      var ch = inner[i];
      if (ch === '\\') {
        i++;
        continue;
      }
      if (OPENERS[ch] && match[i] > i) {
        i = match[i];
        continue;
      }
      if (ch === '<') {
        var close = inner.indexOf('>', i);
        if (close > i) {
          i = close;
          continue;
        }
      }
      if (ch === ':') colon = i;
    }
    if (colon < 0) return null;
    var num = inner.slice(colon + 1).trim();
    if (!NUMBER_RE.test(num)) return null;
    return {
      base: inner.slice(0, colon).trim(),
      weight: Number.parseFloat(num),
      raw: num
    };
  }
  function hasTopLevel(inner, chars) {
    var _scanBrackets3 = scanBrackets(inner),
      match = _scanBrackets3.match;
    for (var i = 0; i < inner.length; i++) {
      var ch = inner[i];
      if (ch === '\\') {
        i++;
        continue;
      }
      if (OPENERS[ch] && match[i] > i) {
        i = match[i];
        continue;
      }
      if (chars.includes(ch)) return true;
    }
    return false;
  }
  function isWrapped(s, opener) {
    if (s.length < 2 || s[0] !== opener) return false;
    var _scanBrackets4 = scanBrackets(s),
      match = _scanBrackets4.match;
    return match[0] === s.length - 1;
  }
  var round2 = function round2(x) {
    return Math.round(x * 100) / 100;
  };
  var round1 = function round1(x) {
    return Math.round(x * 10) / 10;
  };
  function formatWeight(w) {
    var r = round2(w);
    return String(Object.is(r, -0) ? 0 : r);
  }
  function parseLora(core) {
    var m = /^<([A-Za-z][\w-]*):([^>]*)>$/.exec(core);
    if (!m) return null;
    var type = m[1];
    if (!LORA_TYPES_RE.test(type)) return {
      type: type,
      extra: true
    };
    var parts = m[2].split(':');
    var name = parts[0];
    var strengthRaw = parts.length > 1 ? parts[1].trim() : '';
    var rest = parts.slice(2);
    var strengthOk = strengthRaw === '' || NUMBER_RE.test(strengthRaw);
    return {
      type: type,
      name: name,
      strengthRaw: strengthRaw,
      rest: rest,
      weight: strengthRaw !== '' && strengthOk ? Number.parseFloat(strengthRaw) : 1,
      broken: !name.trim() || !strengthOk
    };
  }
  function classifySegment(core) {
    var text = String(core || '');
    var words = text.trim() ? text.trim().split(/\s+/).length : 0;
    var common = {
      text: text,
      words: words,
      sentence: words >= 4
    };
    if (KEYWORDS.includes(text)) {
      return _objectSpread(_objectSpread({}, common), {}, {
        kind: 'keyword',
        keyword: text,
        weight: null,
        explicit: false,
        base: text,
        sentence: false
      });
    }
    if (text[0] === '<' && text[text.length - 1] === '>') {
      var lora = parseLora(text);
      if (lora && !lora.extra) {
        return _objectSpread(_objectSpread({}, common), {}, {
          kind: 'lora',
          weight: lora.weight,
          explicit: lora.strengthRaw !== '',
          base: lora.name,
          lora: lora,
          sentence: false
        });
      }
      if (lora && lora.extra) {
        return _objectSpread(_objectSpread({}, common), {}, {
          kind: 'extra',
          weight: null,
          explicit: false,
          base: text,
          sentence: false
        });
      }
    }
    if (isWrapped(text, '(')) {
      var inner = text.slice(1, -1);
      var exp = splitExplicitWeight(inner);
      if (exp) {
        return _objectSpread(_objectSpread({}, common), {}, {
          kind: 'weighted',
          weight: exp.weight,
          explicit: true,
          base: exp.base
        });
      }
      var depth = 1;
      var body = inner;
      while (isWrapped(body.trim(), '(') && !splitExplicitWeight(body.trim().slice(1, -1))) {
        body = body.trim().slice(1, -1);
        depth++;
      }
      return _objectSpread(_objectSpread({}, common), {}, {
        kind: 'weighted',
        weight: round2(Math.pow(1.1, depth)),
        explicit: false,
        nest: depth,
        base: body.trim()
      });
    }
    if (isWrapped(text, '[')) {
      var _inner = text.slice(1, -1);
      if (hasTopLevel(_inner, [':', '|'])) {
        return _objectSpread(_objectSpread({}, common), {}, {
          kind: 'schedule',
          weight: null,
          explicit: false,
          base: text,
          sentence: false
        });
      }
      var _depth = 1;
      var _body = _inner;
      while (isWrapped(_body.trim(), '[') && !hasTopLevel(_body.trim().slice(1, -1), [':', '|'])) {
        _body = _body.trim().slice(1, -1);
        _depth++;
      }
      return _objectSpread(_objectSpread({}, common), {}, {
        kind: 'weighted',
        weight: round2(1 / Math.pow(1.1, _depth)),
        explicit: false,
        nest: _depth,
        bracket: true,
        base: _body.trim()
      });
    }
    if (isWrapped(text, '{')) {
      return _objectSpread(_objectSpread({}, common), {}, {
        kind: 'group',
        weight: null,
        explicit: false,
        base: text,
        sentence: false
      });
    }
    return _objectSpread(_objectSpread({}, common), {}, {
      kind: 'tag',
      weight: null,
      explicit: false,
      base: text
    });
  }
  var clamp = function clamp(x, lo, hi) {
    return Math.min(hi, Math.max(lo, x));
  };
  function getSegmentWeight(core) {
    var info = classifySegment(core);
    if (info.kind === 'keyword' || info.kind === 'extra') return null;
    if (info.kind === 'lora') return info.weight;
    if (info.kind === 'weighted') return info.explicit ? info.weight : round1(info.weight);
    return 1;
  }
  function setSegmentWeight(core, w) {
    var info = classifySegment(core);
    if (info.kind === 'keyword' || info.kind === 'extra') return null;
    if (info.kind === 'lora') {
      var _info$lora = info.lora,
        type = _info$lora.type,
        name = _info$lora.name,
        rest = _info$lora.rest;
      var value = formatWeight(clamp(w, LORA_MIN, LORA_MAX));
      return "<".concat(type, ":").concat(name, ":").concat(value).concat(rest.length ? ":".concat(rest.join(':')) : '', ">");
    }
    var weight = round2(clamp(w, WEIGHT_MIN, WEIGHT_MAX));
    var base = info.kind === 'weighted' ? info.base : info.text.trim();
    if (Math.abs(weight - 1) < 1e-9) {
      if (segmentPrompt(base).length <= 1 && !/^\s*$/.test(base)) return base;
      return "(".concat(base, ":1)");
    }
    return "(".concat(base, ":").concat(formatWeight(weight), ")");
  }
  function adjustSegmentWeight(core, delta) {
    var current = getSegmentWeight(core);
    if (current === null) return null;
    var info = classifySegment(core);
    var lo = info.kind === 'lora' ? LORA_MIN : WEIGHT_MIN;
    var hi = info.kind === 'lora' ? LORA_MAX : WEIGHT_MAX;
    var next = clamp(round2(current + delta), lo, hi);
    if (next === current && info.kind !== 'weighted') return core;
    return setSegmentWeight(core, next);
  }
  function normalizeKey(core) {
    var info = typeof core === 'string' ? classifySegment(core) : core;
    if (!info || info.kind === 'keyword') return '';
    if (info.kind === 'lora') return "<lora:".concat(String(info.lora.name).trim().toLowerCase(), ">");
    return String(info.base || '').replace(/\\([()[\]{}])/g, '$1').replace(/_/g, ' ').replace(/\s+/g, ' ').trim().toLowerCase();
  }
  function lintPrompt(text, segments) {
    var src = typeof text === 'string' ? text : '';
    var segs = segments || segmentPrompt(src);
    var issues = segs.map(function () {
      return null;
    });
    var add = function add(i, issue) {
      if (i < 0) return;
      if (!issues[i]) issues[i] = [];
      if (!issues[i].some(function (x) {
        return x.code === issue.code;
      })) issues[i].push(issue);
    };
    var _scanBrackets5 = scanBrackets(src),
      unmatched = _scanBrackets5.unmatched,
      brokenAngles = _scanBrackets5.brokenAngles;
    var locate = function locate(pos) {
      for (var i = 0; i < segs.length; i++) {
        if (pos >= segs[i].start && pos < segs[i].end) return i;
      }
      return -1;
    };
    unmatched.forEach(function (pos) {
      return add(locate(pos), {
        code: 'unbalanced',
        char: src[pos],
        pos: pos
      });
    });
    brokenAngles.forEach(function (pos) {
      return add(locate(pos), {
        code: 'brokenLora',
        pos: pos
      });
    });
    var seen = new Map();
    segs.forEach(function (seg, i) {
      if (seg.kind === 'lora') {
        if (seg.lora.broken) add(i, {
          code: 'brokenLora'
        });else if (seg.weight < LORA_MIN || seg.weight > LORA_MAX) add(i, {
          code: 'weightRange',
          weight: seg.weight
        });
      } else if (seg.kind === 'weighted' && seg.explicit) {
        if (seg.weight < WEIGHT_MIN || seg.weight > WEIGHT_MAX) add(i, {
          code: 'weightRange',
          weight: seg.weight
        });
      }
      var key = normalizeKey(seg);
      if (!key) return;
      if (seen.has(key)) add(i, {
        code: 'duplicate',
        first: seen.get(key)
      });else seen.set(key, i);
    });
    return issues;
  }
  function replaceRange(text, start, end, insert) {
    return text.slice(0, start) + insert + text.slice(end);
  }
  function replaceSegment(text, segments, i, core) {
    var seg = segments[i];
    return {
      text: replaceRange(text, seg.start, seg.end, core),
      start: seg.start,
      end: seg.start + core.length
    };
  }
  function deleteSegment(text, segments, i) {
    var seg = segments[i];
    var next = segments[i + 1];
    var prev = segments[i - 1];
    if (next) {
      var gapAfter = text.slice(seg.end, next.start);
      var gapBefore = prev ? text.slice(prev.end, seg.start) : '';
      if (prev && gapAfter.includes('\n') && !gapBefore.includes('\n')) {
        return {
          text: replaceRange(text, prev.end, seg.end, ''),
          caret: prev.end
        };
      }
      return {
        text: replaceRange(text, seg.start, next.start, ''),
        caret: seg.start
      };
    }
    if (prev) {
      return {
        text: replaceRange(text, prev.end, seg.end, ''),
        caret: prev.end
      };
    }
    var out = replaceRange(text, seg.start, seg.end, '');
    return {
      text: out.trim() ? out : '',
      caret: out.trim() ? seg.start : 0
    };
  }
  function cleanInsert(s) {
    return String(s || '').replace(/^[\s,;，；]+/, '').replace(/[\s,;，；]+$/, '');
  }
  function insertAfterSegment(text, segments, i, insert) {
    var piece = cleanInsert(insert);
    if (!piece) return null;
    if (!segments.length || i < 0) return appendSegment(text, piece);
    var seg = segments[i];
    var joined = ", ".concat(piece);
    return {
      text: replaceRange(text, seg.end, seg.end, joined),
      start: seg.end + 2,
      end: seg.end + joined.length
    };
  }
  function duplicateSegment(text, segments, i) {
    return insertAfterSegment(text, segments, i, segments[i].text);
  }
  function appendSegment(text, piece) {
    var src = typeof text === 'string' ? text : '';
    var clean = cleanInsert(piece);
    if (!clean) return {
      text: src,
      start: src.length,
      end: src.length
    };
    var trimmed = src.replace(/\s+$/, '');
    if (!trimmed) return {
      text: clean,
      start: 0,
      end: clean.length
    };
    var sep = /[,;，；]$/.test(trimmed) ? ' ' : ', ';
    var out = trimmed + sep + clean;
    return {
      text: out,
      start: out.length - clean.length,
      end: out.length
    };
  }
  function moveSegment(text, segments, from, to) {
    var count = segments.length;
    if (from < 0 || from >= count) return null;
    var target = Math.max(0, Math.min(count - 1, to));
    if (target === from) return null;
    var cores = segments.map(function (s) {
      return s.text;
    });
    var _cores$splice = cores.splice(from, 1),
      _cores$splice2 = _slicedToArray(_cores$splice, 1),
      moved = _cores$splice2[0];
    cores.splice(target, 0, moved);
    var out = '';
    var cursor = 0;
    var newStart = 0;
    segments.forEach(function (seg, idx) {
      out += text.slice(cursor, seg.start);
      if (idx === target) newStart = out.length;
      out += cores[idx];
      cursor = seg.end;
    });
    out += text.slice(cursor);
    return {
      text: out,
      index: target,
      start: newStart,
      end: newStart + moved.length
    };
  }
  function toggleUnderscores(core) {
    var info = classifySegment(core);
    if (info.kind === 'lora' || info.kind === 'extra' || info.kind === 'keyword') return null;
    if (core.includes('_')) {
      return core.replace(/((?:[0-9A-Za-z\xAA\xB2\xB3\xB5\xB9\xBA\xBC-\xBE\xC0-\xD6\xD8-\xF6\xF8-\u02C1\u02C6-\u02D1\u02E0-\u02E4\u02EC\u02EE\u0370-\u0374\u0376\u0377\u037A-\u037D\u037F\u0386\u0388-\u038A\u038C\u038E-\u03A1\u03A3-\u03F5\u03F7-\u0481\u048A-\u052F\u0531-\u0556\u0558\u0559\u0560-\u0588\u058B\u058C\u05D0-\u05EA\u05EF-\u05F2\u0620-\u064A\u0660-\u0669\u066E\u066F\u0671-\u06D3\u06D5\u06E5\u06E6\u06EE-\u06FC\u06FF\u0710\u0712-\u072F\u074D-\u07A5\u07B1\u07C0-\u07EA\u07F4\u07F5\u07FA\u0800-\u0815\u081A\u0824\u0828\u0840-\u0858\u0860-\u086A\u0870-\u0887\u0889-\u088F\u08A0-\u08C9\u0904-\u0939\u093D\u0950\u0958-\u0961\u0966-\u096F\u0971-\u0980\u0985-\u098C\u098F\u0990\u0993-\u09A8\u09AA-\u09B0\u09B2\u09B6-\u09B9\u09BD\u09CE\u09DC\u09DD\u09DF-\u09E1\u09E6-\u09F1\u09F4-\u09F9\u09FC\u0A05-\u0A0A\u0A0F\u0A10\u0A13-\u0A28\u0A2A-\u0A30\u0A32\u0A33\u0A35\u0A36\u0A38\u0A39\u0A59-\u0A5C\u0A5E\u0A66-\u0A6F\u0A72-\u0A74\u0A85-\u0A8D\u0A8F-\u0A91\u0A93-\u0AA8\u0AAA-\u0AB0\u0AB2\u0AB3\u0AB5-\u0AB9\u0ABD\u0AD0\u0AE0\u0AE1\u0AE6-\u0AEF\u0AF9\u0B05-\u0B0C\u0B0F\u0B10\u0B13-\u0B28\u0B2A-\u0B30\u0B32\u0B33\u0B35-\u0B39\u0B3D\u0B5C\u0B5D\u0B5F-\u0B61\u0B66-\u0B6F\u0B71-\u0B77\u0B83\u0B85-\u0B8A\u0B8E-\u0B90\u0B92-\u0B95\u0B99\u0B9A\u0B9C\u0B9E\u0B9F\u0BA3\u0BA4\u0BA8-\u0BAA\u0BAE-\u0BB9\u0BD0\u0BE6-\u0BF2\u0C05-\u0C0C\u0C0E-\u0C10\u0C12-\u0C28\u0C2A-\u0C39\u0C3D\u0C58-\u0C5A\u0C5C\u0C5D\u0C60\u0C61\u0C66-\u0C6F\u0C78-\u0C7E\u0C80\u0C85-\u0C8C\u0C8E-\u0C90\u0C92-\u0CA8\u0CAA-\u0CB3\u0CB5-\u0CB9\u0CBD\u0CDC-\u0CDE\u0CE0\u0CE1\u0CE6-\u0CEF\u0CF1\u0CF2\u0D04-\u0D0C\u0D0E-\u0D10\u0D12-\u0D3A\u0D3D\u0D4E\u0D54-\u0D56\u0D58-\u0D61\u0D66-\u0D78\u0D7A-\u0D7F\u0D85-\u0D96\u0D9A-\u0DB1\u0DB3-\u0DBB\u0DBD\u0DC0-\u0DC6\u0DE6-\u0DEF\u0E01-\u0E30\u0E32\u0E33\u0E40-\u0E46\u0E50-\u0E59\u0E81\u0E82\u0E84\u0E86-\u0E8A\u0E8C-\u0EA3\u0EA5\u0EA7-\u0EB0\u0EB2\u0EB3\u0EBD\u0EC0-\u0EC4\u0EC6\u0ED0-\u0ED9\u0EDC-\u0EDF\u0F00\u0F20-\u0F33\u0F40-\u0F47\u0F49-\u0F6C\u0F88-\u0F8C\u1000-\u102A\u103F-\u1049\u1050-\u1055\u105A-\u105D\u1061\u1065\u1066\u106E-\u1070\u1075-\u1081\u108E\u1090-\u1099\u10A0-\u10C5\u10C7\u10CD\u10D0-\u10FA\u10FC-\u1248\u124A-\u124D\u1250-\u1256\u1258\u125A-\u125D\u1260-\u1288\u128A-\u128D\u1290-\u12B0\u12B2-\u12B5\u12B8-\u12BE\u12C0\u12C2-\u12C5\u12C8-\u12D6\u12D8-\u1310\u1312-\u1315\u1318-\u135A\u1369-\u137C\u1380-\u138F\u13A0-\u13F5\u13F8-\u13FD\u1401-\u166C\u166F-\u167F\u1681-\u169A\u16A0-\u16EA\u16EE-\u16F8\u1700-\u1711\u171F-\u1731\u1740-\u1751\u1760-\u176C\u176E-\u1770\u1780-\u17B3\u17D7\u17DC\u17E0-\u17E9\u17F0-\u17F9\u1810-\u1819\u1820-\u1878\u1880-\u1884\u1887-\u18A8\u18AA\u18B0-\u18F5\u1900-\u191E\u1946-\u196D\u1970-\u1974\u1980-\u19AB\u19B0-\u19C9\u19D0-\u19DA\u1A00-\u1A16\u1A20-\u1A54\u1A80-\u1A89\u1A90-\u1A99\u1AA7\u1B05-\u1B33\u1B45-\u1B4C\u1B50-\u1B59\u1B83-\u1BA0\u1BAE-\u1BE5\u1C00-\u1C23\u1C40-\u1C49\u1C4D-\u1C7D\u1C80-\u1C8A\u1C90-\u1CBA\u1CBD-\u1CBF\u1CE9-\u1CEC\u1CEE-\u1CF3\u1CF5\u1CF6\u1CFA\u1D00-\u1DBF\u1E00-\u1F15\u1F18-\u1F1D\u1F20-\u1F45\u1F48-\u1F4D\u1F50-\u1F57\u1F59\u1F5B\u1F5D\u1F5F-\u1F7D\u1F80-\u1FB4\u1FB6-\u1FBC\u1FBE\u1FC2-\u1FC4\u1FC6-\u1FCC\u1FD0-\u1FD3\u1FD6-\u1FDB\u1FE0-\u1FEC\u1FF2-\u1FF4\u1FF6-\u1FFC\u2070\u2071\u2074-\u2079\u207F-\u2089\u208F-\u209F\u2102\u2107\u210A-\u2113\u2115\u2119-\u211D\u2124\u2126\u2128\u212A-\u212D\u212F-\u2139\u213C-\u213F\u2145-\u2149\u214E\u2150-\u2189\u2460-\u249B\u24EA-\u24FF\u2776-\u2793\u2C00-\u2CE4\u2CEB-\u2CEE\u2CF2\u2CF3\u2CFD\u2D00-\u2D25\u2D27\u2D2D\u2D30-\u2D67\u2D6F\u2D80-\u2D96\u2DA0-\u2DA6\u2DA8-\u2DAE\u2DB0-\u2DB6\u2DB8-\u2DBE\u2DC0-\u2DC6\u2DC8-\u2DCE\u2DD0-\u2DD6\u2DD8-\u2DDE\u2E2F\u3005-\u3007\u3021-\u3029\u3031-\u3035\u3038-\u303C\u3041-\u3096\u309D-\u309F\u30A1-\u30FA\u30FC-\u30FF\u3105-\u312F\u3131-\u318E\u3192-\u3195\u31A0-\u31BF\u31F0-\u31FF\u3220-\u3229\u3248-\u324F\u3251-\u325F\u3280-\u3289\u32B1-\u32BF\u3400-\u4DBF\u4E00-\uA48C\uA4D0-\uA4FD\uA500-\uA60C\uA610-\uA62B\uA640-\uA66E\uA67F-\uA69D\uA6A0-\uA6EF\uA717-\uA71F\uA722-\uA788\uA78B-\uA7DD\uA7E2\uA7F1-\uA801\uA803-\uA805\uA807-\uA80A\uA80C-\uA822\uA830-\uA835\uA840-\uA873\uA882-\uA8B3\uA8D0-\uA8D9\uA8F2-\uA8F7\uA8FB\uA8FD\uA8FE\uA900-\uA925\uA930-\uA946\uA960-\uA97C\uA984-\uA9B2\uA9CF-\uA9D9\uA9E0-\uA9E4\uA9E6-\uA9FE\uAA00-\uAA28\uAA40-\uAA42\uAA44-\uAA4B\uAA50-\uAA59\uAA60-\uAA76\uAA7A\uAA7E-\uAAAF\uAAB1\uAAB5\uAAB6\uAAB9-\uAABD\uAAC0\uAAC2\uAADB-\uAADD\uAAE0-\uAAEA\uAAF2-\uAAF4\uAB01-\uAB06\uAB09-\uAB0E\uAB11-\uAB16\uAB20-\uAB26\uAB28-\uAB2E\uAB30-\uAB5A\uAB5C-\uAB69\uAB6C\uAB6D\uAB70-\uABE2\uABF0-\uABF9\uAC00-\uD7A3\uD7B0-\uD7C6\uD7CB-\uD7FB\uF900-\uFA6D\uFA70-\uFAD9\uFB00-\uFB06\uFB13-\uFB17\uFB1D\uFB1F-\uFB28\uFB2A-\uFB36\uFB38-\uFB3C\uFB3E\uFB40\uFB41\uFB43\uFB44\uFB46-\uFBB1\uFBD3-\uFD3D\uFD50-\uFD8F\uFD92-\uFDC7\uFDF0-\uFDFB\uFE70-\uFE74\uFE76-\uFEFC\uFF10-\uFF19\uFF21-\uFF3A\uFF41-\uFF5A\uFF66-\uFFBE\uFFC2-\uFFC7\uFFCA-\uFFCF\uFFD2-\uFFD7\uFFDA-\uFFDC]|\uD800[\uDC00-\uDC0B\uDC0D-\uDC26\uDC28-\uDC3A\uDC3C\uDC3D\uDC3F-\uDC4D\uDC50-\uDC5D\uDC80-\uDCFA\uDD07-\uDD33\uDD40-\uDD78\uDD8A\uDD8B\uDE80-\uDE9C\uDEA0-\uDED0\uDEE1-\uDEFB\uDF00-\uDF23\uDF2D-\uDF4A\uDF50-\uDF75\uDF80-\uDF9D\uDFA0-\uDFC3\uDFC8-\uDFCF\uDFD1-\uDFD5]|\uD801[\uDC00-\uDC9D\uDCA0-\uDCA9\uDCB0-\uDCD3\uDCD8-\uDCFB\uDD00-\uDD27\uDD30-\uDD63\uDD70-\uDD7A\uDD7C-\uDD8A\uDD8C-\uDD92\uDD94\uDD95\uDD97-\uDDA1\uDDA3-\uDDB1\uDDB3-\uDDB9\uDDBB\uDDBC\uDDC0-\uDDF3\uDE00-\uDF36\uDF40-\uDF55\uDF60-\uDF67\uDF80-\uDF85\uDF87-\uDFB0\uDFB2-\uDFBF]|\uD802[\uDC00-\uDC05\uDC08\uDC0A-\uDC35\uDC37\uDC38\uDC3C\uDC3F-\uDC55\uDC58-\uDC76\uDC79-\uDC9E\uDCA7-\uDCAF\uDCE0-\uDCF2\uDCF4\uDCF5\uDCFB-\uDD1B\uDD20-\uDD39\uDD40-\uDD59\uDD80-\uDDB7\uDDBC-\uDDCF\uDDD2-\uDE00\uDE10-\uDE13\uDE15-\uDE17\uDE19-\uDE35\uDE40-\uDE48\uDE60-\uDE7E\uDE80-\uDE9F\uDEC0-\uDEC7\uDEC9-\uDEE4\uDEEB-\uDEEF\uDF00-\uDF35\uDF40-\uDF55\uDF58-\uDF72\uDF78-\uDF91\uDFA9-\uDFAF]|\uD803[\uDC00-\uDC48\uDC80-\uDCB2\uDCC0-\uDCF2\uDCFA-\uDD23\uDD30-\uDD39\uDD40-\uDD65\uDD6F-\uDD85\uDE60-\uDE7E\uDE80-\uDEA9\uDEB0\uDEB1\uDEC2-\uDEC7\uDED9-\uDEEE\uDF00-\uDF27\uDF30-\uDF45\uDF51-\uDF54\uDF70-\uDF81\uDFB0-\uDFCB\uDFE0-\uDFF6]|\uD804[\uDC03-\uDC37\uDC52-\uDC6F\uDC71\uDC72\uDC75\uDC83-\uDCAF\uDCD0-\uDCE8\uDCF0-\uDCF9\uDD03-\uDD26\uDD36-\uDD3F\uDD44\uDD47\uDD50-\uDD72\uDD76\uDD83-\uDDB2\uDDC1-\uDDC4\uDDD0-\uDDDA\uDDDC\uDDE1-\uDDF4\uDE00-\uDE11\uDE13-\uDE2B\uDE3F\uDE40\uDE80-\uDE86\uDE88\uDE8A-\uDE8D\uDE8F-\uDE9D\uDE9F-\uDEA8\uDEB0-\uDEDE\uDEF0-\uDEF9\uDF05-\uDF0C\uDF0F\uDF10\uDF13-\uDF28\uDF2A-\uDF30\uDF32\uDF33\uDF35-\uDF39\uDF3D\uDF50\uDF5D-\uDF61\uDF80-\uDF89\uDF8B\uDF8E\uDF90-\uDFB5\uDFB7\uDFD1\uDFD3]|\uD805[\uDC00-\uDC34\uDC47-\uDC4A\uDC50-\uDC59\uDC5F-\uDC61\uDC80-\uDCAF\uDCC4\uDCC5\uDCC7\uDCD0-\uDCD9\uDD80-\uDDAE\uDDD8-\uDDDB\uDE00-\uDE2F\uDE44\uDE50-\uDE59\uDE80-\uDEAA\uDEB8\uDEC0-\uDEC9\uDED0-\uDEE3\uDF00-\uDF1A\uDF30-\uDF3B\uDF40-\uDF46]|\uD806[\uDC00-\uDC2B\uDCA0-\uDCF2\uDCFF-\uDD06\uDD09\uDD0C-\uDD13\uDD15\uDD16\uDD18-\uDD2F\uDD3F\uDD41\uDD50-\uDD59\uDDA0-\uDDA7\uDDAA-\uDDD0\uDDE1\uDDE3\uDE00\uDE0B-\uDE32\uDE3A\uDE50\uDE5C-\uDE89\uDE9D\uDEB0-\uDEF8\uDF0A\uDFC0-\uDFE0\uDFF0-\uDFF9]|\uD807[\uDC00-\uDC08\uDC0A-\uDC2E\uDC40\uDC50-\uDC6C\uDC72-\uDC8F\uDD00-\uDD06\uDD08\uDD09\uDD0B-\uDD30\uDD46\uDD50-\uDD59\uDD60-\uDD65\uDD67\uDD68\uDD6A-\uDD89\uDD98\uDDA0-\uDDA9\uDDB0-\uDDDB\uDDE0-\uDDE9\uDDF1\uDEE0-\uDEF2\uDF02\uDF04-\uDF10\uDF12-\uDF33\uDF50-\uDF59\uDFB0\uDFC0-\uDFD4]|\uD808[\uDC00-\uDF99]|\uD809[\uDC00-\uDC6F\uDC75-\uDD43\uDD50-\uDE86]|\uD80B[\uDF90-\uDFF0]|[\uD80C\uD80E\uD80F\uD81C-\uD822\uD840-\uD868\uD86A-\uD86D\uD86F-\uD872\uD874-\uD879\uD880-\uD883\uD885-\uD88C\uD8B4-\uD8BE][\uDC00-\uDFFF]|\uD80D[\uDC00-\uDC2F\uDC41-\uDC46\uDC60-\uDFFF]|\uD810[\uDC00-\uDFFA]|\uD811[\uDC00-\uDE46]|\uD818[\uDD00-\uDD1D\uDD30-\uDD39]|\uD81A[\uDC00-\uDE38\uDE40-\uDE5E\uDE60-\uDE69\uDE70-\uDEBE\uDEC0-\uDEC9\uDED0-\uDEED\uDF00-\uDF2F\uDF40-\uDF43\uDF50-\uDF59\uDF5B-\uDF61\uDF63-\uDF77\uDF7D-\uDF8F]|\uD81B[\uDD40-\uDD6C\uDD70-\uDD79\uDE40-\uDE96\uDEA0-\uDEB8\uDEBB-\uDED3\uDF00-\uDF4A\uDF50\uDF93-\uDF9F\uDFE0\uDFE1\uDFE3\uDFF2-\uDFF6]|\uD823[\uDC00-\uDCDA\uDCFF-\uDD20\uDD80-\uDDF2\uDE00-\uDFFF]|\uD824[\uDC00-\uDD91\uDDA0-\uDDD2]|\uD82B[\uDFF0-\uDFF3\uDFF5-\uDFFB\uDFFD\uDFFE]|\uD82C[\uDC00-\uDD28\uDD32\uDD50-\uDD52\uDD55\uDD64-\uDD68\uDD70-\uDEFB]|\uD82F[\uDC00-\uDC6A\uDC70-\uDC7C\uDC80-\uDC88\uDC90-\uDC99]|\uD833[\uDCF0-\uDCF9]|\uD834[\uDEC0-\uDED3\uDEE0-\uDEF3\uDF60-\uDF78]|\uD835[\uDC00-\uDC54\uDC56-\uDC9C\uDC9E\uDC9F\uDCA2\uDCA5\uDCA6\uDCA9-\uDCAC\uDCAE-\uDCB9\uDCBB\uDCBD-\uDCC3\uDCC5-\uDD05\uDD07-\uDD0A\uDD0D-\uDD14\uDD16-\uDD1C\uDD1E-\uDD39\uDD3B-\uDD3E\uDD40-\uDD44\uDD46\uDD4A-\uDD50\uDD52-\uDEA6\uDEA8-\uDEC0\uDEC2-\uDEDA\uDEDC-\uDEFA\uDEFC-\uDF14\uDF16-\uDF34\uDF36-\uDF4E\uDF50-\uDF6E\uDF70-\uDF88\uDF8A-\uDFA8\uDFAA-\uDFC2\uDFC4-\uDFCB\uDFCE-\uDFFF]|\uD837[\uDF00-\uDF81\uDF90-\uDF96\uDFCD-\uDFFF]|\uD838[\uDC30-\uDC6D\uDD00-\uDD2C\uDD37-\uDD3D\uDD40-\uDD49\uDD4E\uDE90-\uDEAD\uDEC0-\uDEEB\uDEF0-\uDEF9]|\uD839[\uDCD0-\uDCEB\uDCF0-\uDCF9\uDDD0-\uDDED\uDDF0-\uDDFA\uDEC0-\uDEDE\uDEE0-\uDEE2\uDEE4\uDEE5\uDEE7-\uDEED\uDEF0-\uDEF4\uDEFE\uDEFF\uDFE0-\uDFE6\uDFE8-\uDFEB\uDFED\uDFEE\uDFF0-\uDFFE]|\uD83A[\uDC00-\uDCC4\uDCC7-\uDCCF\uDD00-\uDD43\uDD4B\uDD50-\uDD59]|\uD83B[\uDC71-\uDCAB\uDCAD-\uDCAF\uDCB1-\uDCB4\uDD01-\uDD2D\uDD2F-\uDD3D\uDE00-\uDE03\uDE05-\uDE1F\uDE21\uDE22\uDE24\uDE27\uDE29-\uDE32\uDE34-\uDE37\uDE39\uDE3B\uDE42\uDE47\uDE49\uDE4B\uDE4D-\uDE4F\uDE51\uDE52\uDE54\uDE57\uDE59\uDE5B\uDE5D\uDE5F\uDE61\uDE62\uDE64\uDE67-\uDE6A\uDE6C-\uDE72\uDE74-\uDE77\uDE79-\uDE7C\uDE7E\uDE80-\uDE89\uDE8B-\uDE9B\uDEA1-\uDEA3\uDEA5-\uDEA9\uDEAB-\uDEBB]|\uD83C[\uDD00-\uDD0C]|\uD83E[\uDFF0-\uDFF9]|\uD869[\uDC00-\uDEDF\uDF00-\uDFFF]|\uD86E[\uDC00-\uDC1E\uDC20-\uDFFF]|\uD873[\uDC00-\uDEAD\uDEB0-\uDFFF]|\uD87A[\uDC00-\uDFE0\uDFF0-\uDFFF]|\uD87B[\uDC00-\uDE5D]|\uD87E[\uDC00-\uDE1D]|\uD884[\uDC00-\uDF4A\uDF50-\uDFFF]|\uD88D[\uDC00-\uDC79]|\uD8BF[\uDC00-\uDC3F]))_+(?=(?:[0-9A-Za-z\xAA\xB2\xB3\xB5\xB9\xBA\xBC-\xBE\xC0-\xD6\xD8-\xF6\xF8-\u02C1\u02C6-\u02D1\u02E0-\u02E4\u02EC\u02EE\u0370-\u0374\u0376\u0377\u037A-\u037D\u037F\u0386\u0388-\u038A\u038C\u038E-\u03A1\u03A3-\u03F5\u03F7-\u0481\u048A-\u052F\u0531-\u0556\u0558\u0559\u0560-\u0588\u058B\u058C\u05D0-\u05EA\u05EF-\u05F2\u0620-\u064A\u0660-\u0669\u066E\u066F\u0671-\u06D3\u06D5\u06E5\u06E6\u06EE-\u06FC\u06FF\u0710\u0712-\u072F\u074D-\u07A5\u07B1\u07C0-\u07EA\u07F4\u07F5\u07FA\u0800-\u0815\u081A\u0824\u0828\u0840-\u0858\u0860-\u086A\u0870-\u0887\u0889-\u088F\u08A0-\u08C9\u0904-\u0939\u093D\u0950\u0958-\u0961\u0966-\u096F\u0971-\u0980\u0985-\u098C\u098F\u0990\u0993-\u09A8\u09AA-\u09B0\u09B2\u09B6-\u09B9\u09BD\u09CE\u09DC\u09DD\u09DF-\u09E1\u09E6-\u09F1\u09F4-\u09F9\u09FC\u0A05-\u0A0A\u0A0F\u0A10\u0A13-\u0A28\u0A2A-\u0A30\u0A32\u0A33\u0A35\u0A36\u0A38\u0A39\u0A59-\u0A5C\u0A5E\u0A66-\u0A6F\u0A72-\u0A74\u0A85-\u0A8D\u0A8F-\u0A91\u0A93-\u0AA8\u0AAA-\u0AB0\u0AB2\u0AB3\u0AB5-\u0AB9\u0ABD\u0AD0\u0AE0\u0AE1\u0AE6-\u0AEF\u0AF9\u0B05-\u0B0C\u0B0F\u0B10\u0B13-\u0B28\u0B2A-\u0B30\u0B32\u0B33\u0B35-\u0B39\u0B3D\u0B5C\u0B5D\u0B5F-\u0B61\u0B66-\u0B6F\u0B71-\u0B77\u0B83\u0B85-\u0B8A\u0B8E-\u0B90\u0B92-\u0B95\u0B99\u0B9A\u0B9C\u0B9E\u0B9F\u0BA3\u0BA4\u0BA8-\u0BAA\u0BAE-\u0BB9\u0BD0\u0BE6-\u0BF2\u0C05-\u0C0C\u0C0E-\u0C10\u0C12-\u0C28\u0C2A-\u0C39\u0C3D\u0C58-\u0C5A\u0C5C\u0C5D\u0C60\u0C61\u0C66-\u0C6F\u0C78-\u0C7E\u0C80\u0C85-\u0C8C\u0C8E-\u0C90\u0C92-\u0CA8\u0CAA-\u0CB3\u0CB5-\u0CB9\u0CBD\u0CDC-\u0CDE\u0CE0\u0CE1\u0CE6-\u0CEF\u0CF1\u0CF2\u0D04-\u0D0C\u0D0E-\u0D10\u0D12-\u0D3A\u0D3D\u0D4E\u0D54-\u0D56\u0D58-\u0D61\u0D66-\u0D78\u0D7A-\u0D7F\u0D85-\u0D96\u0D9A-\u0DB1\u0DB3-\u0DBB\u0DBD\u0DC0-\u0DC6\u0DE6-\u0DEF\u0E01-\u0E30\u0E32\u0E33\u0E40-\u0E46\u0E50-\u0E59\u0E81\u0E82\u0E84\u0E86-\u0E8A\u0E8C-\u0EA3\u0EA5\u0EA7-\u0EB0\u0EB2\u0EB3\u0EBD\u0EC0-\u0EC4\u0EC6\u0ED0-\u0ED9\u0EDC-\u0EDF\u0F00\u0F20-\u0F33\u0F40-\u0F47\u0F49-\u0F6C\u0F88-\u0F8C\u1000-\u102A\u103F-\u1049\u1050-\u1055\u105A-\u105D\u1061\u1065\u1066\u106E-\u1070\u1075-\u1081\u108E\u1090-\u1099\u10A0-\u10C5\u10C7\u10CD\u10D0-\u10FA\u10FC-\u1248\u124A-\u124D\u1250-\u1256\u1258\u125A-\u125D\u1260-\u1288\u128A-\u128D\u1290-\u12B0\u12B2-\u12B5\u12B8-\u12BE\u12C0\u12C2-\u12C5\u12C8-\u12D6\u12D8-\u1310\u1312-\u1315\u1318-\u135A\u1369-\u137C\u1380-\u138F\u13A0-\u13F5\u13F8-\u13FD\u1401-\u166C\u166F-\u167F\u1681-\u169A\u16A0-\u16EA\u16EE-\u16F8\u1700-\u1711\u171F-\u1731\u1740-\u1751\u1760-\u176C\u176E-\u1770\u1780-\u17B3\u17D7\u17DC\u17E0-\u17E9\u17F0-\u17F9\u1810-\u1819\u1820-\u1878\u1880-\u1884\u1887-\u18A8\u18AA\u18B0-\u18F5\u1900-\u191E\u1946-\u196D\u1970-\u1974\u1980-\u19AB\u19B0-\u19C9\u19D0-\u19DA\u1A00-\u1A16\u1A20-\u1A54\u1A80-\u1A89\u1A90-\u1A99\u1AA7\u1B05-\u1B33\u1B45-\u1B4C\u1B50-\u1B59\u1B83-\u1BA0\u1BAE-\u1BE5\u1C00-\u1C23\u1C40-\u1C49\u1C4D-\u1C7D\u1C80-\u1C8A\u1C90-\u1CBA\u1CBD-\u1CBF\u1CE9-\u1CEC\u1CEE-\u1CF3\u1CF5\u1CF6\u1CFA\u1D00-\u1DBF\u1E00-\u1F15\u1F18-\u1F1D\u1F20-\u1F45\u1F48-\u1F4D\u1F50-\u1F57\u1F59\u1F5B\u1F5D\u1F5F-\u1F7D\u1F80-\u1FB4\u1FB6-\u1FBC\u1FBE\u1FC2-\u1FC4\u1FC6-\u1FCC\u1FD0-\u1FD3\u1FD6-\u1FDB\u1FE0-\u1FEC\u1FF2-\u1FF4\u1FF6-\u1FFC\u2070\u2071\u2074-\u2079\u207F-\u2089\u208F-\u209F\u2102\u2107\u210A-\u2113\u2115\u2119-\u211D\u2124\u2126\u2128\u212A-\u212D\u212F-\u2139\u213C-\u213F\u2145-\u2149\u214E\u2150-\u2189\u2460-\u249B\u24EA-\u24FF\u2776-\u2793\u2C00-\u2CE4\u2CEB-\u2CEE\u2CF2\u2CF3\u2CFD\u2D00-\u2D25\u2D27\u2D2D\u2D30-\u2D67\u2D6F\u2D80-\u2D96\u2DA0-\u2DA6\u2DA8-\u2DAE\u2DB0-\u2DB6\u2DB8-\u2DBE\u2DC0-\u2DC6\u2DC8-\u2DCE\u2DD0-\u2DD6\u2DD8-\u2DDE\u2E2F\u3005-\u3007\u3021-\u3029\u3031-\u3035\u3038-\u303C\u3041-\u3096\u309D-\u309F\u30A1-\u30FA\u30FC-\u30FF\u3105-\u312F\u3131-\u318E\u3192-\u3195\u31A0-\u31BF\u31F0-\u31FF\u3220-\u3229\u3248-\u324F\u3251-\u325F\u3280-\u3289\u32B1-\u32BF\u3400-\u4DBF\u4E00-\uA48C\uA4D0-\uA4FD\uA500-\uA60C\uA610-\uA62B\uA640-\uA66E\uA67F-\uA69D\uA6A0-\uA6EF\uA717-\uA71F\uA722-\uA788\uA78B-\uA7DD\uA7E2\uA7F1-\uA801\uA803-\uA805\uA807-\uA80A\uA80C-\uA822\uA830-\uA835\uA840-\uA873\uA882-\uA8B3\uA8D0-\uA8D9\uA8F2-\uA8F7\uA8FB\uA8FD\uA8FE\uA900-\uA925\uA930-\uA946\uA960-\uA97C\uA984-\uA9B2\uA9CF-\uA9D9\uA9E0-\uA9E4\uA9E6-\uA9FE\uAA00-\uAA28\uAA40-\uAA42\uAA44-\uAA4B\uAA50-\uAA59\uAA60-\uAA76\uAA7A\uAA7E-\uAAAF\uAAB1\uAAB5\uAAB6\uAAB9-\uAABD\uAAC0\uAAC2\uAADB-\uAADD\uAAE0-\uAAEA\uAAF2-\uAAF4\uAB01-\uAB06\uAB09-\uAB0E\uAB11-\uAB16\uAB20-\uAB26\uAB28-\uAB2E\uAB30-\uAB5A\uAB5C-\uAB69\uAB6C\uAB6D\uAB70-\uABE2\uABF0-\uABF9\uAC00-\uD7A3\uD7B0-\uD7C6\uD7CB-\uD7FB\uF900-\uFA6D\uFA70-\uFAD9\uFB00-\uFB06\uFB13-\uFB17\uFB1D\uFB1F-\uFB28\uFB2A-\uFB36\uFB38-\uFB3C\uFB3E\uFB40\uFB41\uFB43\uFB44\uFB46-\uFBB1\uFBD3-\uFD3D\uFD50-\uFD8F\uFD92-\uFDC7\uFDF0-\uFDFB\uFE70-\uFE74\uFE76-\uFEFC\uFF10-\uFF19\uFF21-\uFF3A\uFF41-\uFF5A\uFF66-\uFFBE\uFFC2-\uFFC7\uFFCA-\uFFCF\uFFD2-\uFFD7\uFFDA-\uFFDC]|\uD800[\uDC00-\uDC0B\uDC0D-\uDC26\uDC28-\uDC3A\uDC3C\uDC3D\uDC3F-\uDC4D\uDC50-\uDC5D\uDC80-\uDCFA\uDD07-\uDD33\uDD40-\uDD78\uDD8A\uDD8B\uDE80-\uDE9C\uDEA0-\uDED0\uDEE1-\uDEFB\uDF00-\uDF23\uDF2D-\uDF4A\uDF50-\uDF75\uDF80-\uDF9D\uDFA0-\uDFC3\uDFC8-\uDFCF\uDFD1-\uDFD5]|\uD801[\uDC00-\uDC9D\uDCA0-\uDCA9\uDCB0-\uDCD3\uDCD8-\uDCFB\uDD00-\uDD27\uDD30-\uDD63\uDD70-\uDD7A\uDD7C-\uDD8A\uDD8C-\uDD92\uDD94\uDD95\uDD97-\uDDA1\uDDA3-\uDDB1\uDDB3-\uDDB9\uDDBB\uDDBC\uDDC0-\uDDF3\uDE00-\uDF36\uDF40-\uDF55\uDF60-\uDF67\uDF80-\uDF85\uDF87-\uDFB0\uDFB2-\uDFBF]|\uD802[\uDC00-\uDC05\uDC08\uDC0A-\uDC35\uDC37\uDC38\uDC3C\uDC3F-\uDC55\uDC58-\uDC76\uDC79-\uDC9E\uDCA7-\uDCAF\uDCE0-\uDCF2\uDCF4\uDCF5\uDCFB-\uDD1B\uDD20-\uDD39\uDD40-\uDD59\uDD80-\uDDB7\uDDBC-\uDDCF\uDDD2-\uDE00\uDE10-\uDE13\uDE15-\uDE17\uDE19-\uDE35\uDE40-\uDE48\uDE60-\uDE7E\uDE80-\uDE9F\uDEC0-\uDEC7\uDEC9-\uDEE4\uDEEB-\uDEEF\uDF00-\uDF35\uDF40-\uDF55\uDF58-\uDF72\uDF78-\uDF91\uDFA9-\uDFAF]|\uD803[\uDC00-\uDC48\uDC80-\uDCB2\uDCC0-\uDCF2\uDCFA-\uDD23\uDD30-\uDD39\uDD40-\uDD65\uDD6F-\uDD85\uDE60-\uDE7E\uDE80-\uDEA9\uDEB0\uDEB1\uDEC2-\uDEC7\uDED9-\uDEEE\uDF00-\uDF27\uDF30-\uDF45\uDF51-\uDF54\uDF70-\uDF81\uDFB0-\uDFCB\uDFE0-\uDFF6]|\uD804[\uDC03-\uDC37\uDC52-\uDC6F\uDC71\uDC72\uDC75\uDC83-\uDCAF\uDCD0-\uDCE8\uDCF0-\uDCF9\uDD03-\uDD26\uDD36-\uDD3F\uDD44\uDD47\uDD50-\uDD72\uDD76\uDD83-\uDDB2\uDDC1-\uDDC4\uDDD0-\uDDDA\uDDDC\uDDE1-\uDDF4\uDE00-\uDE11\uDE13-\uDE2B\uDE3F\uDE40\uDE80-\uDE86\uDE88\uDE8A-\uDE8D\uDE8F-\uDE9D\uDE9F-\uDEA8\uDEB0-\uDEDE\uDEF0-\uDEF9\uDF05-\uDF0C\uDF0F\uDF10\uDF13-\uDF28\uDF2A-\uDF30\uDF32\uDF33\uDF35-\uDF39\uDF3D\uDF50\uDF5D-\uDF61\uDF80-\uDF89\uDF8B\uDF8E\uDF90-\uDFB5\uDFB7\uDFD1\uDFD3]|\uD805[\uDC00-\uDC34\uDC47-\uDC4A\uDC50-\uDC59\uDC5F-\uDC61\uDC80-\uDCAF\uDCC4\uDCC5\uDCC7\uDCD0-\uDCD9\uDD80-\uDDAE\uDDD8-\uDDDB\uDE00-\uDE2F\uDE44\uDE50-\uDE59\uDE80-\uDEAA\uDEB8\uDEC0-\uDEC9\uDED0-\uDEE3\uDF00-\uDF1A\uDF30-\uDF3B\uDF40-\uDF46]|\uD806[\uDC00-\uDC2B\uDCA0-\uDCF2\uDCFF-\uDD06\uDD09\uDD0C-\uDD13\uDD15\uDD16\uDD18-\uDD2F\uDD3F\uDD41\uDD50-\uDD59\uDDA0-\uDDA7\uDDAA-\uDDD0\uDDE1\uDDE3\uDE00\uDE0B-\uDE32\uDE3A\uDE50\uDE5C-\uDE89\uDE9D\uDEB0-\uDEF8\uDF0A\uDFC0-\uDFE0\uDFF0-\uDFF9]|\uD807[\uDC00-\uDC08\uDC0A-\uDC2E\uDC40\uDC50-\uDC6C\uDC72-\uDC8F\uDD00-\uDD06\uDD08\uDD09\uDD0B-\uDD30\uDD46\uDD50-\uDD59\uDD60-\uDD65\uDD67\uDD68\uDD6A-\uDD89\uDD98\uDDA0-\uDDA9\uDDB0-\uDDDB\uDDE0-\uDDE9\uDDF1\uDEE0-\uDEF2\uDF02\uDF04-\uDF10\uDF12-\uDF33\uDF50-\uDF59\uDFB0\uDFC0-\uDFD4]|\uD808[\uDC00-\uDF99]|\uD809[\uDC00-\uDC6F\uDC75-\uDD43\uDD50-\uDE86]|\uD80B[\uDF90-\uDFF0]|[\uD80C\uD80E\uD80F\uD81C-\uD822\uD840-\uD868\uD86A-\uD86D\uD86F-\uD872\uD874-\uD879\uD880-\uD883\uD885-\uD88C\uD8B4-\uD8BE][\uDC00-\uDFFF]|\uD80D[\uDC00-\uDC2F\uDC41-\uDC46\uDC60-\uDFFF]|\uD810[\uDC00-\uDFFA]|\uD811[\uDC00-\uDE46]|\uD818[\uDD00-\uDD1D\uDD30-\uDD39]|\uD81A[\uDC00-\uDE38\uDE40-\uDE5E\uDE60-\uDE69\uDE70-\uDEBE\uDEC0-\uDEC9\uDED0-\uDEED\uDF00-\uDF2F\uDF40-\uDF43\uDF50-\uDF59\uDF5B-\uDF61\uDF63-\uDF77\uDF7D-\uDF8F]|\uD81B[\uDD40-\uDD6C\uDD70-\uDD79\uDE40-\uDE96\uDEA0-\uDEB8\uDEBB-\uDED3\uDF00-\uDF4A\uDF50\uDF93-\uDF9F\uDFE0\uDFE1\uDFE3\uDFF2-\uDFF6]|\uD823[\uDC00-\uDCDA\uDCFF-\uDD20\uDD80-\uDDF2\uDE00-\uDFFF]|\uD824[\uDC00-\uDD91\uDDA0-\uDDD2]|\uD82B[\uDFF0-\uDFF3\uDFF5-\uDFFB\uDFFD\uDFFE]|\uD82C[\uDC00-\uDD28\uDD32\uDD50-\uDD52\uDD55\uDD64-\uDD68\uDD70-\uDEFB]|\uD82F[\uDC00-\uDC6A\uDC70-\uDC7C\uDC80-\uDC88\uDC90-\uDC99]|\uD833[\uDCF0-\uDCF9]|\uD834[\uDEC0-\uDED3\uDEE0-\uDEF3\uDF60-\uDF78]|\uD835[\uDC00-\uDC54\uDC56-\uDC9C\uDC9E\uDC9F\uDCA2\uDCA5\uDCA6\uDCA9-\uDCAC\uDCAE-\uDCB9\uDCBB\uDCBD-\uDCC3\uDCC5-\uDD05\uDD07-\uDD0A\uDD0D-\uDD14\uDD16-\uDD1C\uDD1E-\uDD39\uDD3B-\uDD3E\uDD40-\uDD44\uDD46\uDD4A-\uDD50\uDD52-\uDEA6\uDEA8-\uDEC0\uDEC2-\uDEDA\uDEDC-\uDEFA\uDEFC-\uDF14\uDF16-\uDF34\uDF36-\uDF4E\uDF50-\uDF6E\uDF70-\uDF88\uDF8A-\uDFA8\uDFAA-\uDFC2\uDFC4-\uDFCB\uDFCE-\uDFFF]|\uD837[\uDF00-\uDF81\uDF90-\uDF96\uDFCD-\uDFFF]|\uD838[\uDC30-\uDC6D\uDD00-\uDD2C\uDD37-\uDD3D\uDD40-\uDD49\uDD4E\uDE90-\uDEAD\uDEC0-\uDEEB\uDEF0-\uDEF9]|\uD839[\uDCD0-\uDCEB\uDCF0-\uDCF9\uDDD0-\uDDED\uDDF0-\uDDFA\uDEC0-\uDEDE\uDEE0-\uDEE2\uDEE4\uDEE5\uDEE7-\uDEED\uDEF0-\uDEF4\uDEFE\uDEFF\uDFE0-\uDFE6\uDFE8-\uDFEB\uDFED\uDFEE\uDFF0-\uDFFE]|\uD83A[\uDC00-\uDCC4\uDCC7-\uDCCF\uDD00-\uDD43\uDD4B\uDD50-\uDD59]|\uD83B[\uDC71-\uDCAB\uDCAD-\uDCAF\uDCB1-\uDCB4\uDD01-\uDD2D\uDD2F-\uDD3D\uDE00-\uDE03\uDE05-\uDE1F\uDE21\uDE22\uDE24\uDE27\uDE29-\uDE32\uDE34-\uDE37\uDE39\uDE3B\uDE42\uDE47\uDE49\uDE4B\uDE4D-\uDE4F\uDE51\uDE52\uDE54\uDE57\uDE59\uDE5B\uDE5D\uDE5F\uDE61\uDE62\uDE64\uDE67-\uDE6A\uDE6C-\uDE72\uDE74-\uDE77\uDE79-\uDE7C\uDE7E\uDE80-\uDE89\uDE8B-\uDE9B\uDEA1-\uDEA3\uDEA5-\uDEA9\uDEAB-\uDEBB]|\uD83C[\uDD00-\uDD0C]|\uD83E[\uDFF0-\uDFF9]|\uD869[\uDC00-\uDEDF\uDF00-\uDFFF]|\uD86E[\uDC00-\uDC1E\uDC20-\uDFFF]|\uD873[\uDC00-\uDEAD\uDEB0-\uDFFF]|\uD87A[\uDC00-\uDFE0\uDFF0-\uDFFF]|\uD87B[\uDC00-\uDE5D]|\uD87E[\uDC00-\uDE1D]|\uD884[\uDC00-\uDF4A\uDF50-\uDFFF]|\uD88D[\uDC00-\uDC79]|\uD8BF[\uDC00-\uDC3F]))/g, '$1 ');
    }
    var out = core.replace(/((?:[0-9A-Za-z\xAA\xB2\xB3\xB5\xB9\xBA\xBC-\xBE\xC0-\xD6\xD8-\xF6\xF8-\u02C1\u02C6-\u02D1\u02E0-\u02E4\u02EC\u02EE\u0370-\u0374\u0376\u0377\u037A-\u037D\u037F\u0386\u0388-\u038A\u038C\u038E-\u03A1\u03A3-\u03F5\u03F7-\u0481\u048A-\u052F\u0531-\u0556\u0558\u0559\u0560-\u0588\u058B\u058C\u05D0-\u05EA\u05EF-\u05F2\u0620-\u064A\u0660-\u0669\u066E\u066F\u0671-\u06D3\u06D5\u06E5\u06E6\u06EE-\u06FC\u06FF\u0710\u0712-\u072F\u074D-\u07A5\u07B1\u07C0-\u07EA\u07F4\u07F5\u07FA\u0800-\u0815\u081A\u0824\u0828\u0840-\u0858\u0860-\u086A\u0870-\u0887\u0889-\u088F\u08A0-\u08C9\u0904-\u0939\u093D\u0950\u0958-\u0961\u0966-\u096F\u0971-\u0980\u0985-\u098C\u098F\u0990\u0993-\u09A8\u09AA-\u09B0\u09B2\u09B6-\u09B9\u09BD\u09CE\u09DC\u09DD\u09DF-\u09E1\u09E6-\u09F1\u09F4-\u09F9\u09FC\u0A05-\u0A0A\u0A0F\u0A10\u0A13-\u0A28\u0A2A-\u0A30\u0A32\u0A33\u0A35\u0A36\u0A38\u0A39\u0A59-\u0A5C\u0A5E\u0A66-\u0A6F\u0A72-\u0A74\u0A85-\u0A8D\u0A8F-\u0A91\u0A93-\u0AA8\u0AAA-\u0AB0\u0AB2\u0AB3\u0AB5-\u0AB9\u0ABD\u0AD0\u0AE0\u0AE1\u0AE6-\u0AEF\u0AF9\u0B05-\u0B0C\u0B0F\u0B10\u0B13-\u0B28\u0B2A-\u0B30\u0B32\u0B33\u0B35-\u0B39\u0B3D\u0B5C\u0B5D\u0B5F-\u0B61\u0B66-\u0B6F\u0B71-\u0B77\u0B83\u0B85-\u0B8A\u0B8E-\u0B90\u0B92-\u0B95\u0B99\u0B9A\u0B9C\u0B9E\u0B9F\u0BA3\u0BA4\u0BA8-\u0BAA\u0BAE-\u0BB9\u0BD0\u0BE6-\u0BF2\u0C05-\u0C0C\u0C0E-\u0C10\u0C12-\u0C28\u0C2A-\u0C39\u0C3D\u0C58-\u0C5A\u0C5C\u0C5D\u0C60\u0C61\u0C66-\u0C6F\u0C78-\u0C7E\u0C80\u0C85-\u0C8C\u0C8E-\u0C90\u0C92-\u0CA8\u0CAA-\u0CB3\u0CB5-\u0CB9\u0CBD\u0CDC-\u0CDE\u0CE0\u0CE1\u0CE6-\u0CEF\u0CF1\u0CF2\u0D04-\u0D0C\u0D0E-\u0D10\u0D12-\u0D3A\u0D3D\u0D4E\u0D54-\u0D56\u0D58-\u0D61\u0D66-\u0D78\u0D7A-\u0D7F\u0D85-\u0D96\u0D9A-\u0DB1\u0DB3-\u0DBB\u0DBD\u0DC0-\u0DC6\u0DE6-\u0DEF\u0E01-\u0E30\u0E32\u0E33\u0E40-\u0E46\u0E50-\u0E59\u0E81\u0E82\u0E84\u0E86-\u0E8A\u0E8C-\u0EA3\u0EA5\u0EA7-\u0EB0\u0EB2\u0EB3\u0EBD\u0EC0-\u0EC4\u0EC6\u0ED0-\u0ED9\u0EDC-\u0EDF\u0F00\u0F20-\u0F33\u0F40-\u0F47\u0F49-\u0F6C\u0F88-\u0F8C\u1000-\u102A\u103F-\u1049\u1050-\u1055\u105A-\u105D\u1061\u1065\u1066\u106E-\u1070\u1075-\u1081\u108E\u1090-\u1099\u10A0-\u10C5\u10C7\u10CD\u10D0-\u10FA\u10FC-\u1248\u124A-\u124D\u1250-\u1256\u1258\u125A-\u125D\u1260-\u1288\u128A-\u128D\u1290-\u12B0\u12B2-\u12B5\u12B8-\u12BE\u12C0\u12C2-\u12C5\u12C8-\u12D6\u12D8-\u1310\u1312-\u1315\u1318-\u135A\u1369-\u137C\u1380-\u138F\u13A0-\u13F5\u13F8-\u13FD\u1401-\u166C\u166F-\u167F\u1681-\u169A\u16A0-\u16EA\u16EE-\u16F8\u1700-\u1711\u171F-\u1731\u1740-\u1751\u1760-\u176C\u176E-\u1770\u1780-\u17B3\u17D7\u17DC\u17E0-\u17E9\u17F0-\u17F9\u1810-\u1819\u1820-\u1878\u1880-\u1884\u1887-\u18A8\u18AA\u18B0-\u18F5\u1900-\u191E\u1946-\u196D\u1970-\u1974\u1980-\u19AB\u19B0-\u19C9\u19D0-\u19DA\u1A00-\u1A16\u1A20-\u1A54\u1A80-\u1A89\u1A90-\u1A99\u1AA7\u1B05-\u1B33\u1B45-\u1B4C\u1B50-\u1B59\u1B83-\u1BA0\u1BAE-\u1BE5\u1C00-\u1C23\u1C40-\u1C49\u1C4D-\u1C7D\u1C80-\u1C8A\u1C90-\u1CBA\u1CBD-\u1CBF\u1CE9-\u1CEC\u1CEE-\u1CF3\u1CF5\u1CF6\u1CFA\u1D00-\u1DBF\u1E00-\u1F15\u1F18-\u1F1D\u1F20-\u1F45\u1F48-\u1F4D\u1F50-\u1F57\u1F59\u1F5B\u1F5D\u1F5F-\u1F7D\u1F80-\u1FB4\u1FB6-\u1FBC\u1FBE\u1FC2-\u1FC4\u1FC6-\u1FCC\u1FD0-\u1FD3\u1FD6-\u1FDB\u1FE0-\u1FEC\u1FF2-\u1FF4\u1FF6-\u1FFC\u2070\u2071\u2074-\u2079\u207F-\u2089\u208F-\u209F\u2102\u2107\u210A-\u2113\u2115\u2119-\u211D\u2124\u2126\u2128\u212A-\u212D\u212F-\u2139\u213C-\u213F\u2145-\u2149\u214E\u2150-\u2189\u2460-\u249B\u24EA-\u24FF\u2776-\u2793\u2C00-\u2CE4\u2CEB-\u2CEE\u2CF2\u2CF3\u2CFD\u2D00-\u2D25\u2D27\u2D2D\u2D30-\u2D67\u2D6F\u2D80-\u2D96\u2DA0-\u2DA6\u2DA8-\u2DAE\u2DB0-\u2DB6\u2DB8-\u2DBE\u2DC0-\u2DC6\u2DC8-\u2DCE\u2DD0-\u2DD6\u2DD8-\u2DDE\u2E2F\u3005-\u3007\u3021-\u3029\u3031-\u3035\u3038-\u303C\u3041-\u3096\u309D-\u309F\u30A1-\u30FA\u30FC-\u30FF\u3105-\u312F\u3131-\u318E\u3192-\u3195\u31A0-\u31BF\u31F0-\u31FF\u3220-\u3229\u3248-\u324F\u3251-\u325F\u3280-\u3289\u32B1-\u32BF\u3400-\u4DBF\u4E00-\uA48C\uA4D0-\uA4FD\uA500-\uA60C\uA610-\uA62B\uA640-\uA66E\uA67F-\uA69D\uA6A0-\uA6EF\uA717-\uA71F\uA722-\uA788\uA78B-\uA7DD\uA7E2\uA7F1-\uA801\uA803-\uA805\uA807-\uA80A\uA80C-\uA822\uA830-\uA835\uA840-\uA873\uA882-\uA8B3\uA8D0-\uA8D9\uA8F2-\uA8F7\uA8FB\uA8FD\uA8FE\uA900-\uA925\uA930-\uA946\uA960-\uA97C\uA984-\uA9B2\uA9CF-\uA9D9\uA9E0-\uA9E4\uA9E6-\uA9FE\uAA00-\uAA28\uAA40-\uAA42\uAA44-\uAA4B\uAA50-\uAA59\uAA60-\uAA76\uAA7A\uAA7E-\uAAAF\uAAB1\uAAB5\uAAB6\uAAB9-\uAABD\uAAC0\uAAC2\uAADB-\uAADD\uAAE0-\uAAEA\uAAF2-\uAAF4\uAB01-\uAB06\uAB09-\uAB0E\uAB11-\uAB16\uAB20-\uAB26\uAB28-\uAB2E\uAB30-\uAB5A\uAB5C-\uAB69\uAB6C\uAB6D\uAB70-\uABE2\uABF0-\uABF9\uAC00-\uD7A3\uD7B0-\uD7C6\uD7CB-\uD7FB\uF900-\uFA6D\uFA70-\uFAD9\uFB00-\uFB06\uFB13-\uFB17\uFB1D\uFB1F-\uFB28\uFB2A-\uFB36\uFB38-\uFB3C\uFB3E\uFB40\uFB41\uFB43\uFB44\uFB46-\uFBB1\uFBD3-\uFD3D\uFD50-\uFD8F\uFD92-\uFDC7\uFDF0-\uFDFB\uFE70-\uFE74\uFE76-\uFEFC\uFF10-\uFF19\uFF21-\uFF3A\uFF41-\uFF5A\uFF66-\uFFBE\uFFC2-\uFFC7\uFFCA-\uFFCF\uFFD2-\uFFD7\uFFDA-\uFFDC]|\uD800[\uDC00-\uDC0B\uDC0D-\uDC26\uDC28-\uDC3A\uDC3C\uDC3D\uDC3F-\uDC4D\uDC50-\uDC5D\uDC80-\uDCFA\uDD07-\uDD33\uDD40-\uDD78\uDD8A\uDD8B\uDE80-\uDE9C\uDEA0-\uDED0\uDEE1-\uDEFB\uDF00-\uDF23\uDF2D-\uDF4A\uDF50-\uDF75\uDF80-\uDF9D\uDFA0-\uDFC3\uDFC8-\uDFCF\uDFD1-\uDFD5]|\uD801[\uDC00-\uDC9D\uDCA0-\uDCA9\uDCB0-\uDCD3\uDCD8-\uDCFB\uDD00-\uDD27\uDD30-\uDD63\uDD70-\uDD7A\uDD7C-\uDD8A\uDD8C-\uDD92\uDD94\uDD95\uDD97-\uDDA1\uDDA3-\uDDB1\uDDB3-\uDDB9\uDDBB\uDDBC\uDDC0-\uDDF3\uDE00-\uDF36\uDF40-\uDF55\uDF60-\uDF67\uDF80-\uDF85\uDF87-\uDFB0\uDFB2-\uDFBF]|\uD802[\uDC00-\uDC05\uDC08\uDC0A-\uDC35\uDC37\uDC38\uDC3C\uDC3F-\uDC55\uDC58-\uDC76\uDC79-\uDC9E\uDCA7-\uDCAF\uDCE0-\uDCF2\uDCF4\uDCF5\uDCFB-\uDD1B\uDD20-\uDD39\uDD40-\uDD59\uDD80-\uDDB7\uDDBC-\uDDCF\uDDD2-\uDE00\uDE10-\uDE13\uDE15-\uDE17\uDE19-\uDE35\uDE40-\uDE48\uDE60-\uDE7E\uDE80-\uDE9F\uDEC0-\uDEC7\uDEC9-\uDEE4\uDEEB-\uDEEF\uDF00-\uDF35\uDF40-\uDF55\uDF58-\uDF72\uDF78-\uDF91\uDFA9-\uDFAF]|\uD803[\uDC00-\uDC48\uDC80-\uDCB2\uDCC0-\uDCF2\uDCFA-\uDD23\uDD30-\uDD39\uDD40-\uDD65\uDD6F-\uDD85\uDE60-\uDE7E\uDE80-\uDEA9\uDEB0\uDEB1\uDEC2-\uDEC7\uDED9-\uDEEE\uDF00-\uDF27\uDF30-\uDF45\uDF51-\uDF54\uDF70-\uDF81\uDFB0-\uDFCB\uDFE0-\uDFF6]|\uD804[\uDC03-\uDC37\uDC52-\uDC6F\uDC71\uDC72\uDC75\uDC83-\uDCAF\uDCD0-\uDCE8\uDCF0-\uDCF9\uDD03-\uDD26\uDD36-\uDD3F\uDD44\uDD47\uDD50-\uDD72\uDD76\uDD83-\uDDB2\uDDC1-\uDDC4\uDDD0-\uDDDA\uDDDC\uDDE1-\uDDF4\uDE00-\uDE11\uDE13-\uDE2B\uDE3F\uDE40\uDE80-\uDE86\uDE88\uDE8A-\uDE8D\uDE8F-\uDE9D\uDE9F-\uDEA8\uDEB0-\uDEDE\uDEF0-\uDEF9\uDF05-\uDF0C\uDF0F\uDF10\uDF13-\uDF28\uDF2A-\uDF30\uDF32\uDF33\uDF35-\uDF39\uDF3D\uDF50\uDF5D-\uDF61\uDF80-\uDF89\uDF8B\uDF8E\uDF90-\uDFB5\uDFB7\uDFD1\uDFD3]|\uD805[\uDC00-\uDC34\uDC47-\uDC4A\uDC50-\uDC59\uDC5F-\uDC61\uDC80-\uDCAF\uDCC4\uDCC5\uDCC7\uDCD0-\uDCD9\uDD80-\uDDAE\uDDD8-\uDDDB\uDE00-\uDE2F\uDE44\uDE50-\uDE59\uDE80-\uDEAA\uDEB8\uDEC0-\uDEC9\uDED0-\uDEE3\uDF00-\uDF1A\uDF30-\uDF3B\uDF40-\uDF46]|\uD806[\uDC00-\uDC2B\uDCA0-\uDCF2\uDCFF-\uDD06\uDD09\uDD0C-\uDD13\uDD15\uDD16\uDD18-\uDD2F\uDD3F\uDD41\uDD50-\uDD59\uDDA0-\uDDA7\uDDAA-\uDDD0\uDDE1\uDDE3\uDE00\uDE0B-\uDE32\uDE3A\uDE50\uDE5C-\uDE89\uDE9D\uDEB0-\uDEF8\uDF0A\uDFC0-\uDFE0\uDFF0-\uDFF9]|\uD807[\uDC00-\uDC08\uDC0A-\uDC2E\uDC40\uDC50-\uDC6C\uDC72-\uDC8F\uDD00-\uDD06\uDD08\uDD09\uDD0B-\uDD30\uDD46\uDD50-\uDD59\uDD60-\uDD65\uDD67\uDD68\uDD6A-\uDD89\uDD98\uDDA0-\uDDA9\uDDB0-\uDDDB\uDDE0-\uDDE9\uDDF1\uDEE0-\uDEF2\uDF02\uDF04-\uDF10\uDF12-\uDF33\uDF50-\uDF59\uDFB0\uDFC0-\uDFD4]|\uD808[\uDC00-\uDF99]|\uD809[\uDC00-\uDC6F\uDC75-\uDD43\uDD50-\uDE86]|\uD80B[\uDF90-\uDFF0]|[\uD80C\uD80E\uD80F\uD81C-\uD822\uD840-\uD868\uD86A-\uD86D\uD86F-\uD872\uD874-\uD879\uD880-\uD883\uD885-\uD88C\uD8B4-\uD8BE][\uDC00-\uDFFF]|\uD80D[\uDC00-\uDC2F\uDC41-\uDC46\uDC60-\uDFFF]|\uD810[\uDC00-\uDFFA]|\uD811[\uDC00-\uDE46]|\uD818[\uDD00-\uDD1D\uDD30-\uDD39]|\uD81A[\uDC00-\uDE38\uDE40-\uDE5E\uDE60-\uDE69\uDE70-\uDEBE\uDEC0-\uDEC9\uDED0-\uDEED\uDF00-\uDF2F\uDF40-\uDF43\uDF50-\uDF59\uDF5B-\uDF61\uDF63-\uDF77\uDF7D-\uDF8F]|\uD81B[\uDD40-\uDD6C\uDD70-\uDD79\uDE40-\uDE96\uDEA0-\uDEB8\uDEBB-\uDED3\uDF00-\uDF4A\uDF50\uDF93-\uDF9F\uDFE0\uDFE1\uDFE3\uDFF2-\uDFF6]|\uD823[\uDC00-\uDCDA\uDCFF-\uDD20\uDD80-\uDDF2\uDE00-\uDFFF]|\uD824[\uDC00-\uDD91\uDDA0-\uDDD2]|\uD82B[\uDFF0-\uDFF3\uDFF5-\uDFFB\uDFFD\uDFFE]|\uD82C[\uDC00-\uDD28\uDD32\uDD50-\uDD52\uDD55\uDD64-\uDD68\uDD70-\uDEFB]|\uD82F[\uDC00-\uDC6A\uDC70-\uDC7C\uDC80-\uDC88\uDC90-\uDC99]|\uD833[\uDCF0-\uDCF9]|\uD834[\uDEC0-\uDED3\uDEE0-\uDEF3\uDF60-\uDF78]|\uD835[\uDC00-\uDC54\uDC56-\uDC9C\uDC9E\uDC9F\uDCA2\uDCA5\uDCA6\uDCA9-\uDCAC\uDCAE-\uDCB9\uDCBB\uDCBD-\uDCC3\uDCC5-\uDD05\uDD07-\uDD0A\uDD0D-\uDD14\uDD16-\uDD1C\uDD1E-\uDD39\uDD3B-\uDD3E\uDD40-\uDD44\uDD46\uDD4A-\uDD50\uDD52-\uDEA6\uDEA8-\uDEC0\uDEC2-\uDEDA\uDEDC-\uDEFA\uDEFC-\uDF14\uDF16-\uDF34\uDF36-\uDF4E\uDF50-\uDF6E\uDF70-\uDF88\uDF8A-\uDFA8\uDFAA-\uDFC2\uDFC4-\uDFCB\uDFCE-\uDFFF]|\uD837[\uDF00-\uDF81\uDF90-\uDF96\uDFCD-\uDFFF]|\uD838[\uDC30-\uDC6D\uDD00-\uDD2C\uDD37-\uDD3D\uDD40-\uDD49\uDD4E\uDE90-\uDEAD\uDEC0-\uDEEB\uDEF0-\uDEF9]|\uD839[\uDCD0-\uDCEB\uDCF0-\uDCF9\uDDD0-\uDDED\uDDF0-\uDDFA\uDEC0-\uDEDE\uDEE0-\uDEE2\uDEE4\uDEE5\uDEE7-\uDEED\uDEF0-\uDEF4\uDEFE\uDEFF\uDFE0-\uDFE6\uDFE8-\uDFEB\uDFED\uDFEE\uDFF0-\uDFFE]|\uD83A[\uDC00-\uDCC4\uDCC7-\uDCCF\uDD00-\uDD43\uDD4B\uDD50-\uDD59]|\uD83B[\uDC71-\uDCAB\uDCAD-\uDCAF\uDCB1-\uDCB4\uDD01-\uDD2D\uDD2F-\uDD3D\uDE00-\uDE03\uDE05-\uDE1F\uDE21\uDE22\uDE24\uDE27\uDE29-\uDE32\uDE34-\uDE37\uDE39\uDE3B\uDE42\uDE47\uDE49\uDE4B\uDE4D-\uDE4F\uDE51\uDE52\uDE54\uDE57\uDE59\uDE5B\uDE5D\uDE5F\uDE61\uDE62\uDE64\uDE67-\uDE6A\uDE6C-\uDE72\uDE74-\uDE77\uDE79-\uDE7C\uDE7E\uDE80-\uDE89\uDE8B-\uDE9B\uDEA1-\uDEA3\uDEA5-\uDEA9\uDEAB-\uDEBB]|\uD83C[\uDD00-\uDD0C]|\uD83E[\uDFF0-\uDFF9]|\uD869[\uDC00-\uDEDF\uDF00-\uDFFF]|\uD86E[\uDC00-\uDC1E\uDC20-\uDFFF]|\uD873[\uDC00-\uDEAD\uDEB0-\uDFFF]|\uD87A[\uDC00-\uDFE0\uDFF0-\uDFFF]|\uD87B[\uDC00-\uDE5D]|\uD87E[\uDC00-\uDE1D]|\uD884[\uDC00-\uDF4A\uDF50-\uDFFF]|\uD88D[\uDC00-\uDC79]|\uD8BF[\uDC00-\uDC3F])) +(?=(?:[0-9A-Za-z\xAA\xB2\xB3\xB5\xB9\xBA\xBC-\xBE\xC0-\xD6\xD8-\xF6\xF8-\u02C1\u02C6-\u02D1\u02E0-\u02E4\u02EC\u02EE\u0370-\u0374\u0376\u0377\u037A-\u037D\u037F\u0386\u0388-\u038A\u038C\u038E-\u03A1\u03A3-\u03F5\u03F7-\u0481\u048A-\u052F\u0531-\u0556\u0558\u0559\u0560-\u0588\u058B\u058C\u05D0-\u05EA\u05EF-\u05F2\u0620-\u064A\u0660-\u0669\u066E\u066F\u0671-\u06D3\u06D5\u06E5\u06E6\u06EE-\u06FC\u06FF\u0710\u0712-\u072F\u074D-\u07A5\u07B1\u07C0-\u07EA\u07F4\u07F5\u07FA\u0800-\u0815\u081A\u0824\u0828\u0840-\u0858\u0860-\u086A\u0870-\u0887\u0889-\u088F\u08A0-\u08C9\u0904-\u0939\u093D\u0950\u0958-\u0961\u0966-\u096F\u0971-\u0980\u0985-\u098C\u098F\u0990\u0993-\u09A8\u09AA-\u09B0\u09B2\u09B6-\u09B9\u09BD\u09CE\u09DC\u09DD\u09DF-\u09E1\u09E6-\u09F1\u09F4-\u09F9\u09FC\u0A05-\u0A0A\u0A0F\u0A10\u0A13-\u0A28\u0A2A-\u0A30\u0A32\u0A33\u0A35\u0A36\u0A38\u0A39\u0A59-\u0A5C\u0A5E\u0A66-\u0A6F\u0A72-\u0A74\u0A85-\u0A8D\u0A8F-\u0A91\u0A93-\u0AA8\u0AAA-\u0AB0\u0AB2\u0AB3\u0AB5-\u0AB9\u0ABD\u0AD0\u0AE0\u0AE1\u0AE6-\u0AEF\u0AF9\u0B05-\u0B0C\u0B0F\u0B10\u0B13-\u0B28\u0B2A-\u0B30\u0B32\u0B33\u0B35-\u0B39\u0B3D\u0B5C\u0B5D\u0B5F-\u0B61\u0B66-\u0B6F\u0B71-\u0B77\u0B83\u0B85-\u0B8A\u0B8E-\u0B90\u0B92-\u0B95\u0B99\u0B9A\u0B9C\u0B9E\u0B9F\u0BA3\u0BA4\u0BA8-\u0BAA\u0BAE-\u0BB9\u0BD0\u0BE6-\u0BF2\u0C05-\u0C0C\u0C0E-\u0C10\u0C12-\u0C28\u0C2A-\u0C39\u0C3D\u0C58-\u0C5A\u0C5C\u0C5D\u0C60\u0C61\u0C66-\u0C6F\u0C78-\u0C7E\u0C80\u0C85-\u0C8C\u0C8E-\u0C90\u0C92-\u0CA8\u0CAA-\u0CB3\u0CB5-\u0CB9\u0CBD\u0CDC-\u0CDE\u0CE0\u0CE1\u0CE6-\u0CEF\u0CF1\u0CF2\u0D04-\u0D0C\u0D0E-\u0D10\u0D12-\u0D3A\u0D3D\u0D4E\u0D54-\u0D56\u0D58-\u0D61\u0D66-\u0D78\u0D7A-\u0D7F\u0D85-\u0D96\u0D9A-\u0DB1\u0DB3-\u0DBB\u0DBD\u0DC0-\u0DC6\u0DE6-\u0DEF\u0E01-\u0E30\u0E32\u0E33\u0E40-\u0E46\u0E50-\u0E59\u0E81\u0E82\u0E84\u0E86-\u0E8A\u0E8C-\u0EA3\u0EA5\u0EA7-\u0EB0\u0EB2\u0EB3\u0EBD\u0EC0-\u0EC4\u0EC6\u0ED0-\u0ED9\u0EDC-\u0EDF\u0F00\u0F20-\u0F33\u0F40-\u0F47\u0F49-\u0F6C\u0F88-\u0F8C\u1000-\u102A\u103F-\u1049\u1050-\u1055\u105A-\u105D\u1061\u1065\u1066\u106E-\u1070\u1075-\u1081\u108E\u1090-\u1099\u10A0-\u10C5\u10C7\u10CD\u10D0-\u10FA\u10FC-\u1248\u124A-\u124D\u1250-\u1256\u1258\u125A-\u125D\u1260-\u1288\u128A-\u128D\u1290-\u12B0\u12B2-\u12B5\u12B8-\u12BE\u12C0\u12C2-\u12C5\u12C8-\u12D6\u12D8-\u1310\u1312-\u1315\u1318-\u135A\u1369-\u137C\u1380-\u138F\u13A0-\u13F5\u13F8-\u13FD\u1401-\u166C\u166F-\u167F\u1681-\u169A\u16A0-\u16EA\u16EE-\u16F8\u1700-\u1711\u171F-\u1731\u1740-\u1751\u1760-\u176C\u176E-\u1770\u1780-\u17B3\u17D7\u17DC\u17E0-\u17E9\u17F0-\u17F9\u1810-\u1819\u1820-\u1878\u1880-\u1884\u1887-\u18A8\u18AA\u18B0-\u18F5\u1900-\u191E\u1946-\u196D\u1970-\u1974\u1980-\u19AB\u19B0-\u19C9\u19D0-\u19DA\u1A00-\u1A16\u1A20-\u1A54\u1A80-\u1A89\u1A90-\u1A99\u1AA7\u1B05-\u1B33\u1B45-\u1B4C\u1B50-\u1B59\u1B83-\u1BA0\u1BAE-\u1BE5\u1C00-\u1C23\u1C40-\u1C49\u1C4D-\u1C7D\u1C80-\u1C8A\u1C90-\u1CBA\u1CBD-\u1CBF\u1CE9-\u1CEC\u1CEE-\u1CF3\u1CF5\u1CF6\u1CFA\u1D00-\u1DBF\u1E00-\u1F15\u1F18-\u1F1D\u1F20-\u1F45\u1F48-\u1F4D\u1F50-\u1F57\u1F59\u1F5B\u1F5D\u1F5F-\u1F7D\u1F80-\u1FB4\u1FB6-\u1FBC\u1FBE\u1FC2-\u1FC4\u1FC6-\u1FCC\u1FD0-\u1FD3\u1FD6-\u1FDB\u1FE0-\u1FEC\u1FF2-\u1FF4\u1FF6-\u1FFC\u2070\u2071\u2074-\u2079\u207F-\u2089\u208F-\u209F\u2102\u2107\u210A-\u2113\u2115\u2119-\u211D\u2124\u2126\u2128\u212A-\u212D\u212F-\u2139\u213C-\u213F\u2145-\u2149\u214E\u2150-\u2189\u2460-\u249B\u24EA-\u24FF\u2776-\u2793\u2C00-\u2CE4\u2CEB-\u2CEE\u2CF2\u2CF3\u2CFD\u2D00-\u2D25\u2D27\u2D2D\u2D30-\u2D67\u2D6F\u2D80-\u2D96\u2DA0-\u2DA6\u2DA8-\u2DAE\u2DB0-\u2DB6\u2DB8-\u2DBE\u2DC0-\u2DC6\u2DC8-\u2DCE\u2DD0-\u2DD6\u2DD8-\u2DDE\u2E2F\u3005-\u3007\u3021-\u3029\u3031-\u3035\u3038-\u303C\u3041-\u3096\u309D-\u309F\u30A1-\u30FA\u30FC-\u30FF\u3105-\u312F\u3131-\u318E\u3192-\u3195\u31A0-\u31BF\u31F0-\u31FF\u3220-\u3229\u3248-\u324F\u3251-\u325F\u3280-\u3289\u32B1-\u32BF\u3400-\u4DBF\u4E00-\uA48C\uA4D0-\uA4FD\uA500-\uA60C\uA610-\uA62B\uA640-\uA66E\uA67F-\uA69D\uA6A0-\uA6EF\uA717-\uA71F\uA722-\uA788\uA78B-\uA7DD\uA7E2\uA7F1-\uA801\uA803-\uA805\uA807-\uA80A\uA80C-\uA822\uA830-\uA835\uA840-\uA873\uA882-\uA8B3\uA8D0-\uA8D9\uA8F2-\uA8F7\uA8FB\uA8FD\uA8FE\uA900-\uA925\uA930-\uA946\uA960-\uA97C\uA984-\uA9B2\uA9CF-\uA9D9\uA9E0-\uA9E4\uA9E6-\uA9FE\uAA00-\uAA28\uAA40-\uAA42\uAA44-\uAA4B\uAA50-\uAA59\uAA60-\uAA76\uAA7A\uAA7E-\uAAAF\uAAB1\uAAB5\uAAB6\uAAB9-\uAABD\uAAC0\uAAC2\uAADB-\uAADD\uAAE0-\uAAEA\uAAF2-\uAAF4\uAB01-\uAB06\uAB09-\uAB0E\uAB11-\uAB16\uAB20-\uAB26\uAB28-\uAB2E\uAB30-\uAB5A\uAB5C-\uAB69\uAB6C\uAB6D\uAB70-\uABE2\uABF0-\uABF9\uAC00-\uD7A3\uD7B0-\uD7C6\uD7CB-\uD7FB\uF900-\uFA6D\uFA70-\uFAD9\uFB00-\uFB06\uFB13-\uFB17\uFB1D\uFB1F-\uFB28\uFB2A-\uFB36\uFB38-\uFB3C\uFB3E\uFB40\uFB41\uFB43\uFB44\uFB46-\uFBB1\uFBD3-\uFD3D\uFD50-\uFD8F\uFD92-\uFDC7\uFDF0-\uFDFB\uFE70-\uFE74\uFE76-\uFEFC\uFF10-\uFF19\uFF21-\uFF3A\uFF41-\uFF5A\uFF66-\uFFBE\uFFC2-\uFFC7\uFFCA-\uFFCF\uFFD2-\uFFD7\uFFDA-\uFFDC]|\uD800[\uDC00-\uDC0B\uDC0D-\uDC26\uDC28-\uDC3A\uDC3C\uDC3D\uDC3F-\uDC4D\uDC50-\uDC5D\uDC80-\uDCFA\uDD07-\uDD33\uDD40-\uDD78\uDD8A\uDD8B\uDE80-\uDE9C\uDEA0-\uDED0\uDEE1-\uDEFB\uDF00-\uDF23\uDF2D-\uDF4A\uDF50-\uDF75\uDF80-\uDF9D\uDFA0-\uDFC3\uDFC8-\uDFCF\uDFD1-\uDFD5]|\uD801[\uDC00-\uDC9D\uDCA0-\uDCA9\uDCB0-\uDCD3\uDCD8-\uDCFB\uDD00-\uDD27\uDD30-\uDD63\uDD70-\uDD7A\uDD7C-\uDD8A\uDD8C-\uDD92\uDD94\uDD95\uDD97-\uDDA1\uDDA3-\uDDB1\uDDB3-\uDDB9\uDDBB\uDDBC\uDDC0-\uDDF3\uDE00-\uDF36\uDF40-\uDF55\uDF60-\uDF67\uDF80-\uDF85\uDF87-\uDFB0\uDFB2-\uDFBF]|\uD802[\uDC00-\uDC05\uDC08\uDC0A-\uDC35\uDC37\uDC38\uDC3C\uDC3F-\uDC55\uDC58-\uDC76\uDC79-\uDC9E\uDCA7-\uDCAF\uDCE0-\uDCF2\uDCF4\uDCF5\uDCFB-\uDD1B\uDD20-\uDD39\uDD40-\uDD59\uDD80-\uDDB7\uDDBC-\uDDCF\uDDD2-\uDE00\uDE10-\uDE13\uDE15-\uDE17\uDE19-\uDE35\uDE40-\uDE48\uDE60-\uDE7E\uDE80-\uDE9F\uDEC0-\uDEC7\uDEC9-\uDEE4\uDEEB-\uDEEF\uDF00-\uDF35\uDF40-\uDF55\uDF58-\uDF72\uDF78-\uDF91\uDFA9-\uDFAF]|\uD803[\uDC00-\uDC48\uDC80-\uDCB2\uDCC0-\uDCF2\uDCFA-\uDD23\uDD30-\uDD39\uDD40-\uDD65\uDD6F-\uDD85\uDE60-\uDE7E\uDE80-\uDEA9\uDEB0\uDEB1\uDEC2-\uDEC7\uDED9-\uDEEE\uDF00-\uDF27\uDF30-\uDF45\uDF51-\uDF54\uDF70-\uDF81\uDFB0-\uDFCB\uDFE0-\uDFF6]|\uD804[\uDC03-\uDC37\uDC52-\uDC6F\uDC71\uDC72\uDC75\uDC83-\uDCAF\uDCD0-\uDCE8\uDCF0-\uDCF9\uDD03-\uDD26\uDD36-\uDD3F\uDD44\uDD47\uDD50-\uDD72\uDD76\uDD83-\uDDB2\uDDC1-\uDDC4\uDDD0-\uDDDA\uDDDC\uDDE1-\uDDF4\uDE00-\uDE11\uDE13-\uDE2B\uDE3F\uDE40\uDE80-\uDE86\uDE88\uDE8A-\uDE8D\uDE8F-\uDE9D\uDE9F-\uDEA8\uDEB0-\uDEDE\uDEF0-\uDEF9\uDF05-\uDF0C\uDF0F\uDF10\uDF13-\uDF28\uDF2A-\uDF30\uDF32\uDF33\uDF35-\uDF39\uDF3D\uDF50\uDF5D-\uDF61\uDF80-\uDF89\uDF8B\uDF8E\uDF90-\uDFB5\uDFB7\uDFD1\uDFD3]|\uD805[\uDC00-\uDC34\uDC47-\uDC4A\uDC50-\uDC59\uDC5F-\uDC61\uDC80-\uDCAF\uDCC4\uDCC5\uDCC7\uDCD0-\uDCD9\uDD80-\uDDAE\uDDD8-\uDDDB\uDE00-\uDE2F\uDE44\uDE50-\uDE59\uDE80-\uDEAA\uDEB8\uDEC0-\uDEC9\uDED0-\uDEE3\uDF00-\uDF1A\uDF30-\uDF3B\uDF40-\uDF46]|\uD806[\uDC00-\uDC2B\uDCA0-\uDCF2\uDCFF-\uDD06\uDD09\uDD0C-\uDD13\uDD15\uDD16\uDD18-\uDD2F\uDD3F\uDD41\uDD50-\uDD59\uDDA0-\uDDA7\uDDAA-\uDDD0\uDDE1\uDDE3\uDE00\uDE0B-\uDE32\uDE3A\uDE50\uDE5C-\uDE89\uDE9D\uDEB0-\uDEF8\uDF0A\uDFC0-\uDFE0\uDFF0-\uDFF9]|\uD807[\uDC00-\uDC08\uDC0A-\uDC2E\uDC40\uDC50-\uDC6C\uDC72-\uDC8F\uDD00-\uDD06\uDD08\uDD09\uDD0B-\uDD30\uDD46\uDD50-\uDD59\uDD60-\uDD65\uDD67\uDD68\uDD6A-\uDD89\uDD98\uDDA0-\uDDA9\uDDB0-\uDDDB\uDDE0-\uDDE9\uDDF1\uDEE0-\uDEF2\uDF02\uDF04-\uDF10\uDF12-\uDF33\uDF50-\uDF59\uDFB0\uDFC0-\uDFD4]|\uD808[\uDC00-\uDF99]|\uD809[\uDC00-\uDC6F\uDC75-\uDD43\uDD50-\uDE86]|\uD80B[\uDF90-\uDFF0]|[\uD80C\uD80E\uD80F\uD81C-\uD822\uD840-\uD868\uD86A-\uD86D\uD86F-\uD872\uD874-\uD879\uD880-\uD883\uD885-\uD88C\uD8B4-\uD8BE][\uDC00-\uDFFF]|\uD80D[\uDC00-\uDC2F\uDC41-\uDC46\uDC60-\uDFFF]|\uD810[\uDC00-\uDFFA]|\uD811[\uDC00-\uDE46]|\uD818[\uDD00-\uDD1D\uDD30-\uDD39]|\uD81A[\uDC00-\uDE38\uDE40-\uDE5E\uDE60-\uDE69\uDE70-\uDEBE\uDEC0-\uDEC9\uDED0-\uDEED\uDF00-\uDF2F\uDF40-\uDF43\uDF50-\uDF59\uDF5B-\uDF61\uDF63-\uDF77\uDF7D-\uDF8F]|\uD81B[\uDD40-\uDD6C\uDD70-\uDD79\uDE40-\uDE96\uDEA0-\uDEB8\uDEBB-\uDED3\uDF00-\uDF4A\uDF50\uDF93-\uDF9F\uDFE0\uDFE1\uDFE3\uDFF2-\uDFF6]|\uD823[\uDC00-\uDCDA\uDCFF-\uDD20\uDD80-\uDDF2\uDE00-\uDFFF]|\uD824[\uDC00-\uDD91\uDDA0-\uDDD2]|\uD82B[\uDFF0-\uDFF3\uDFF5-\uDFFB\uDFFD\uDFFE]|\uD82C[\uDC00-\uDD28\uDD32\uDD50-\uDD52\uDD55\uDD64-\uDD68\uDD70-\uDEFB]|\uD82F[\uDC00-\uDC6A\uDC70-\uDC7C\uDC80-\uDC88\uDC90-\uDC99]|\uD833[\uDCF0-\uDCF9]|\uD834[\uDEC0-\uDED3\uDEE0-\uDEF3\uDF60-\uDF78]|\uD835[\uDC00-\uDC54\uDC56-\uDC9C\uDC9E\uDC9F\uDCA2\uDCA5\uDCA6\uDCA9-\uDCAC\uDCAE-\uDCB9\uDCBB\uDCBD-\uDCC3\uDCC5-\uDD05\uDD07-\uDD0A\uDD0D-\uDD14\uDD16-\uDD1C\uDD1E-\uDD39\uDD3B-\uDD3E\uDD40-\uDD44\uDD46\uDD4A-\uDD50\uDD52-\uDEA6\uDEA8-\uDEC0\uDEC2-\uDEDA\uDEDC-\uDEFA\uDEFC-\uDF14\uDF16-\uDF34\uDF36-\uDF4E\uDF50-\uDF6E\uDF70-\uDF88\uDF8A-\uDFA8\uDFAA-\uDFC2\uDFC4-\uDFCB\uDFCE-\uDFFF]|\uD837[\uDF00-\uDF81\uDF90-\uDF96\uDFCD-\uDFFF]|\uD838[\uDC30-\uDC6D\uDD00-\uDD2C\uDD37-\uDD3D\uDD40-\uDD49\uDD4E\uDE90-\uDEAD\uDEC0-\uDEEB\uDEF0-\uDEF9]|\uD839[\uDCD0-\uDCEB\uDCF0-\uDCF9\uDDD0-\uDDED\uDDF0-\uDDFA\uDEC0-\uDEDE\uDEE0-\uDEE2\uDEE4\uDEE5\uDEE7-\uDEED\uDEF0-\uDEF4\uDEFE\uDEFF\uDFE0-\uDFE6\uDFE8-\uDFEB\uDFED\uDFEE\uDFF0-\uDFFE]|\uD83A[\uDC00-\uDCC4\uDCC7-\uDCCF\uDD00-\uDD43\uDD4B\uDD50-\uDD59]|\uD83B[\uDC71-\uDCAB\uDCAD-\uDCAF\uDCB1-\uDCB4\uDD01-\uDD2D\uDD2F-\uDD3D\uDE00-\uDE03\uDE05-\uDE1F\uDE21\uDE22\uDE24\uDE27\uDE29-\uDE32\uDE34-\uDE37\uDE39\uDE3B\uDE42\uDE47\uDE49\uDE4B\uDE4D-\uDE4F\uDE51\uDE52\uDE54\uDE57\uDE59\uDE5B\uDE5D\uDE5F\uDE61\uDE62\uDE64\uDE67-\uDE6A\uDE6C-\uDE72\uDE74-\uDE77\uDE79-\uDE7C\uDE7E\uDE80-\uDE89\uDE8B-\uDE9B\uDEA1-\uDEA3\uDEA5-\uDEA9\uDEAB-\uDEBB]|\uD83C[\uDD00-\uDD0C]|\uD83E[\uDFF0-\uDFF9]|\uD869[\uDC00-\uDEDF\uDF00-\uDFFF]|\uD86E[\uDC00-\uDC1E\uDC20-\uDFFF]|\uD873[\uDC00-\uDEAD\uDEB0-\uDFFF]|\uD87A[\uDC00-\uDFE0\uDFF0-\uDFFF]|\uD87B[\uDC00-\uDE5D]|\uD87E[\uDC00-\uDE1D]|\uD884[\uDC00-\uDF4A\uDF50-\uDFFF]|\uD88D[\uDC00-\uDC79]|\uD8BF[\uDC00-\uDC3F]))/g, '$1_');
    return out === core ? null : out;
  }
  function escapeTagForPrompt(tag) {
    return String(tag || '').replace(/\\?([()[\]])/g, '\\$1');
  }
  function replaceBase(core, newBase) {
    var info = classifySegment(core);
    var base = cleanInsert(newBase);
    if (!base) return null;
    if (info.kind === 'weighted') {
      var w = info.explicit ? info.weight : round1(info.weight);
      if (Math.abs(w - 1) < 1e-9) return base;
      return "(".concat(base, ":").concat(formatWeight(w), ")");
    }
    return base;
  }
  function autocompleteContext(text, caret) {
    var src = typeof text === 'string' ? text : '';
    if (caret <= 0 || caret > src.length) return null;
    var start = caret;
    while (start > 0) {
      var ch = src[start - 1];
      if (HARD_SEPARATORS.has(ch) || ch === '(' || ch === '[' || ch === '{' || ch === '|' || ch === '>') break;
      if (SOFT_SEPARATORS.has(ch) && start < src.length && isSpace(src[start])) break;
      start--;
    }
    while (start < caret && isSpace(src[start])) start++;
    var query = src.slice(start, caret);
    if (query.length < 2 || query.length > 48) return null;
    if (/[:<\\)\]]/.test(query)) return null;
    if (query.split(/\s+/).length > 4) return null;
    if (KEYWORDS.includes(query.trim())) return null;
    var end = caret;
    while (end < src.length && isWordChar(src[end])) end++;
    var tail = src.slice(end);
    return {
      from: start,
      to: end,
      query: query.replace(/_/g, ' ').toLowerCase(),
      atEnd: /^\s*$/.test(tail)
    };
  }
  var PROMPT_CATEGORIES = ['quality', 'subject', 'character', 'copyright', 'artist', 'trigger', 'species', 'body', 'clothing', 'expression', 'pose', 'interaction', 'setting', 'camera', 'lighting', 'style', 'meta', 'lora', 'unknown'];
  var ORDERS = {
    illustrious: ['quality', 'subject', 'character', 'copyright', 'artist', 'trigger', 'species', 'body', 'clothing', 'expression', 'pose', 'interaction', 'setting', 'camera', 'lighting', 'style', 'meta', 'lora'],
    pony: ['quality', 'subject', 'character', 'copyright', 'trigger', 'species', 'body', 'clothing', 'expression', 'pose', 'interaction', 'setting', 'camera', 'lighting', 'artist', 'style', 'meta', 'lora'],
    anima: ['quality', 'subject', 'character', 'copyright', 'artist', 'trigger', 'species', 'body', 'clothing', 'expression', 'pose', 'interaction', 'setting', 'camera', 'lighting', 'style', 'meta', 'lora']
  };
  var NATURAL_RANK = {
    subject: 0,
    character: 0,
    copyright: 0,
    trigger: 0,
    species: 0,
    body: 1,
    clothing: 1,
    expression: 1,
    pose: 2,
    interaction: 2,
    setting: 3,
    camera: 4,
    lighting: 5,
    style: 6,
    artist: 6,
    quality: 7,
    meta: 7,
    lora: 8
  };
  var QUALITY_SUB = {
    pony: ['score', 'source', 'rating', 'quality', 'aesthetic', 'date', 'resolution', 'embedding'],
    default: ['quality', 'aesthetic', 'date', 'resolution', 'rating', 'score', 'source', 'embedding']
  };
  var ORDER_FAMILIES = ['illustrious', 'pony', 'anima', 'natural'];
  function promptOrderFamily(modelName, familyId) {
    var name = String(modelName || '').toLowerCase();
    var fam = String(familyId || '').toLowerCase();
    if (fam === 'flux' || fam === 'krea' || fam === 'zimage') return 'natural';
    if (fam === 'anima') return 'anima';
    if (/pony|pdxl/.test(name)) return 'pony';
    return 'illustrious';
  }
  var set = function set(s) {
    return new Set(s.split('|').map(function (x) {
      return x.trim();
    }).filter(Boolean));
  };
  var QUALITY_EXACT = set('masterpiece|best quality|high quality|good quality|great quality|amazing quality|normal quality|medium quality|low quality|worst quality|bad quality|top quality|perfect quality|ultra quality|highest quality|highly detailed|ultra detailed|ultra-detailed|extremely detailed|very detailed|detailed|intricate|intricate details|hyperdetailed|hyper detailed|sharp focus|award winning|award-winning|professional|high detail|best|amazing');
  var AESTHETIC_EXACT = set('very aesthetic|aesthetic|displeasing|very displeasing|very awa|worst aesthetic|best aesthetic');
  var DATE_EXACT = set('newest|recent|mid|early|old|oldest');
  var RESOLUTION_EXACT = set('absurdres|highres|incredibly absurdres|lowres|high resolution|ultra high res|ultra high resolution|8k|4k|16k|uhd|hdr|hd|8k uhd|4k uhd');
  var RATING_EXACT = set('general|sensitive|questionable|explicit|nsfw|sfw|safe|suggestive');
  var SUBJECT_EXACT = set('solo|duo|trio|group|solo focus|male focus|female focus|no humans|multiple girls|multiple boys|multiple others|couple|male|female|male/male|male/female|female/female|m/m|f/f|m/f|gynomorph|andromorph|intersex|ambiguous gender|herm|futanari|futa|crowd|solo male|solo female|male solo|female solo');
  var SUBJECT_WORDS = set('man|woman|men|women|boy|girl|boys|girls|guy|guys|lady|person|people|character|warrior|knight|wizard|mage|soldier|king|queen|prince|princess|hero|heroine|adventurer|hunter|barbarian|samurai|ninja|pirate|priest|nun|maid|butler|student|teacher|nurse|doctor|chef|farmer|mercenary|gladiator|viking|monk|paladin|rogue|bard|sorcerer|sorceress|witch|necromancer|shaman|druid|ranger|assassin|thief|guard|gentleman|businessman|athlete|bodybuilder|wrestler|boxer|lifeguard|firefighter|policeman|officer|sailor|astronaut|pilot|twins|siblings|daddy|dad|father|mother|son|daughter|brothers|sisters|lovers|cowboy|cowgirl|lumberjack|biker|trucker|miner|blacksmith|scientist|detective|captain|general|commander|emperor|empress|chieftain|shogun|hunk');
  var SPECIES_WORDS = set('furry|anthro|kemono|feral|humanoid|monster|beast|creature|animal|human|elf|elves|dwarf|halfling|orc|orcs|ork|goblin|hobgoblin|troll|trolls|ogre|giant|demon|devil|imp|angel|vampire|werewolf|lycan|dragon|dragons|wyvern|wolf|wolves|fox|cat|dog|lion|lioness|tiger|leopard|cheetah|panther|jaguar|lynx|bear|panda|horse|stallion|zebra|bull|cow|ox|bison|buffalo|deer|stag|reindeer|moose|elk|rabbit|bunny|hare|bat|bird|eagle|hawk|falcon|owl|raven|crow|parrot|penguin|shark|orca|whale|dolphin|fish|snake|serpent|naga|lizard|gecko|reptile|crocodile|alligator|turtle|tortoise|frog|toad|raccoon|hyena|rat|mouse|squirrel|otter|beaver|badger|skunk|weasel|ferret|pig|boar|hog|goat|sheep|ram|minotaur|centaur|satyr|faun|gryphon|griffon|kobold|gnoll|lizardfolk|lizardman|argonian|khajiit|tauren|draenei|worgen|sergal|protogen|avali|robot|android|cyborg|mecha|alien|slime|ghost|zombie|undead|skeleton|lich|mermaid|merman|canine|canid|feline|felid|equine|bovine|ursine|avian|reptilian|mammal|lupine|vulpine|dinosaur|raptor|kangaroo|koala|monkey|ape|gorilla|elephant|rhino|rhinoceros|hippo|giraffe|camel|jackal|coyote|dingo|husky|catboy|catgirl|dogboy|doggirl|wolfboy|wolfgirl|foxboy|foxgirl|kitsune|nekomimi|tanuki|oni|yokai|fairy|pixie|nymph|golem|gargoyle|insect|spider|arachnid|dragonborn|tiefling|hybrid|chimera|hellhound|cerberus|unicorn|pegasus|phoenix|hydra|pokemon|digimon|yeti|sasquatch|bigfoot');
  var BODY_HEADS = set('hair|hairstyle|haircut|bangs|ponytail|twintails|braid|braids|bun|mohawk|ahoge|sidelocks|dreadlocks|afro|undercut|eyes|eye|pupils|iris|eyebrows|eyelashes|ears|ear|tail|tails|wings|wing|horns|horn|antlers|halo|fur|skin|scales|feathers|body|bodies|build|physique|figure|muscles|muscle|abs|abdomen|pecs|pectorals|chest|biceps|triceps|shoulders|arms|legs|thighs|calves|hips|waist|butt|ass|buttocks|breasts|breast|boobs|nipples|nipple|areolae|penis|cock|dick|balls|testicles|scrotum|sheath|knot|foreskin|glans|pussy|vagina|genitals|anus|crotch|bulge|belly|navel|stomach|gut|paws|paw|claws|claw|fangs|fang|teeth|tusks|tusk|beard|mustache|moustache|stubble|goatee|sideburns|freckles|moles|mole|scar|scars|tattoo|tattoos|markings|stripes|spots|makeup|lipstick|eyeshadow|eyeliner|muzzle|snout|mane|whiskers|lips|nose|cheeks|chin|jaw|neck|hand|hands|fingers|fingernails|nails|feet|foot|toes|soles|hooves|hoof|height|age|complexion|tan|tanline|tanlines|veins|sweat|armpits|armpit|heterochromia|physique|musculature|trapezius|thigh|forearms|forearm|calf|ankles');
  var BODY_WORDS = set('muscular|muscle|slim|slender|skinny|thin|chubby|fat|overweight|plump|obese|curvy|voluptuous|thick|stocky|burly|bulky|beefy|brawny|buff|athletic|toned|petite|tall|short|tiny|mature|elderly|young|adult|bara|hairy|bald|hunky|ripped|shredded|lean|big|huge|large|massive|hyper|pale|dark-skinned|tanned|albino|freckled|bearded');
  var CLOTHING_WORDS = set('nude|naked|topless|bottomless|barefoot|shirtless|pantsless|clothed|clothing|clothes|outfit|costume|attire|garment|underwear|lingerie|undressed|undressing|jewelry|jewellery|necklace|pendant|earrings|earring|piercing|piercings|bracelet|bracelets|ring|rings|anklet|choker|collar|leash|harness|belt|strap|straps|glasses|eyewear|sunglasses|goggles|monocle|eyepatch|mask|headphones|crown|tiara|circlet|hat|cap|beanie|helmet|hood|headband|headwear|headdress|hairband|hairclip|ornament|ribbon|bow|bowtie|necktie|tie|scarf|bandana|bandanna|gloves|glove|gauntlets|bracers|wristband|armband|sleeves|sleeve|cape|cloak|robe|robes|kimono|yukata|hakama|uniform|suit|tuxedo|vest|waistcoat|jacket|coat|trenchcoat|hoodie|sweater|cardigan|shirt|t-shirt|tshirt|blouse|top|tanktop|dress|gown|skirt|miniskirt|pants|trousers|jeans|shorts|leggings|tights|pantyhose|stockings|thighhighs|socks|kneehighs|boots|boot|shoes|shoe|sneakers|heels|sandals|slippers|loafers|panties|bra|briefs|boxers|jockstrap|thong|loincloth|fundoshi|speedo|trunks|bikini|swimsuit|swimwear|leotard|bodysuit|corset|apron|overalls|dungarees|armor|armour|pauldrons|pauldron|breastplate|chainmail|greaves|tabard|sash|backpack|bag|pouch|satchel|holster|quiver|sword|swords|katana|blade|axe|spear|lance|halberd|shield|staff|wand|scepter|arrow|arrows|crossbow|gun|guns|rifle|pistol|revolver|shotgun|weapon|weapons|dagger|knife|hammer|mace|whip|scythe|trident|book|scroll|cup|mug|bottle|phone|smartphone|umbrella|lantern|torch|guitar|microphone|cigarette|cigar|bouquet|flag|banner|chains|jersey|singlet|tunic|poncho|pajamas|bathrobe|towel|diaper|latex|leather|denim|fishnet|lace|spandex|gear|equipment|accessories|accessory|bandages|bandage|wrappings|tassels|cuffs|anklets|headgear|visor|earmuffs|bells|bell|medal|badge|epaulettes|cravat|jabot|ascot');
  var EXPRESSION_EXACT = set('smile|smiling|grin|grinning|smirk|smug|frown|frowning|pout|pouting|blush|blushing|light blush|tears|crying|sobbing|laughing|laugh|open mouth|closed mouth|tongue out|tongue|licking lips|parted lips|clenched teeth|drooling|drool|saliva|ahegao|closed eyes|half-closed eyes|half closed eyes|one eye closed|wink|winking|rolling eyes|wide-eyed|empty eyes|heart-shaped pupils|looking at viewer|looking away|looking back|looking down|looking up|looking at another|looking to the side|looking at self|eye contact|expressionless|bedroom eyes|sweatdrop|heavy breathing|panting|moaning|screaming|shouting|yelling|fangs out|:d|:3|:p|;)|^_^|>_<|o_o|evil smile|evil grin|naughty face|smug face|happy face|seductive smile|gentle smile|teasing|teeth showing');
  var EXPRESSION_WORDS = set('smile|smiling|grin|grinning|smirk|smirking|smug|frown|frowning|pout|blush|blushing|tears|crying|laughing|angry|anger|annoyed|happy|sad|surprised|shocked|scared|afraid|embarrassed|shy|nervous|serious|confident|determined|calm|bored|tired|sleepy|excited|aroused|horny|seductive|flirty|lustful|pleading|expression|expressions|face|gaze|stare|staring|glare|glaring|scowl|scowling|wink|mouth|emotion|mood|cheerful|joyful|grumpy|menacing|fierce|stern|playful|mischievous|sultry|dreamy|melancholic|melancholy|tearful|furious|proud|arrogant|content|relaxed');
  var POSE_EXACT = set('on back|on stomach|on side|on all fours|all fours|arms up|arms behind head|arms behind back|arm up|hand on hip|hands on hips|hand on own chest|crossed arms|arms crossed|crossed legs|legs crossed|spread legs|legs apart|legs up|bent over|arched back|contrapposto|action pose|dynamic pose|fighting stance|battle stance|salute|peace sign|v sign|thumbs up|middle finger|hand up|hands up|outstretched arm|outstretched arms|outstretched hand|head tilt|hands in pockets|hand in pocket|hand on head|hand on face|hand on own face|hands together|presenting|showing off|flexing biceps|double biceps|leg up|knee up|hand on thigh|hand on own thigh|arm behind head|squatting|kneeling|standing|sitting|lying|seiza|wariza|indian style|straddling|stretching|flexing|posing|pose|walking|running|jumping|flying|floating|falling|leaning forward|leaning back|leaning|on top|girl on top|boy on top|male on top|female on top|on lap|sitting on lap|sitting on face|face down ass up|lying on back|lying on side|lying on stomach');
  var POSE_WORDS = set('standing|sitting|kneeling|lying|reclining|squatting|crouching|walking|running|jumping|flying|floating|falling|leaning|bending|stretching|flexing|posing|pose|poses|dancing|fighting|punching|kicking|waving|pointing|reaching|climbing|swimming|sleeping|resting|relaxing|meditating|praying|eating|drinking|smoking|reading|writing|cooking|bathing|showering|holding|lifting|raising|raised|spread|crossed|outstretched|stance|posture|sprawled|lounging|sprinting|charging|attacking|casting|swinging|throwing|drawing|aiming|wielding|carrying|pulling|pushing|riding|stomping|tiptoes|kneel|sit|stand|crouch|squat|stride|striding|strutting|marching|hovering|perched|seated');
  var INTERACTION_EXACT = set('hug|hugging|embrace|embracing|cuddling|cuddle|spooning|kiss|kissing|french kiss|holding hands|hand holding|princess carry|piggyback|arm around shoulder|arm around waist|sex|anal|oral|fellatio|blowjob|cunnilingus|rimming|rimjob|handjob|footjob|paizuri|titjob|frotting|frottage|grinding|penetration|fingering|masturbation|mutual masturbation|threesome|foursome|orgy|group sex|gangbang|doggystyle|missionary|cowgirl position|reverse cowgirl position|mating press|standing sex|69|deepthroat|irrumatio|cum|cumshot|ejaculation|orgasm|creampie|cum inside|cum on body|bukkake|facial|size difference|height difference|vore|fight|battle|duel|arm wrestling|wrestling|tickling|spanking|groping|petting|headpat|feeding|handshake|high five|fist bump|bondage|bdsm|domination|submission|licking|biting|sucking|grabbing|touching');
  var INTERACTION_WORDS = set('sex|hug|hugging|kiss|kissing|cuddling|embrace|embracing|penetration|penetrating|fellatio|blowjob|handjob|footjob|masturbation|masturbating|cum|cumming|ejaculation|orgasm|threesome|orgy|gangbang|vore|groping|grabbing|licking|sucking|biting|touching|spanking|tickling|wrestling|fighting|duel|together|another|other|partner|partners|each|romance|romantic|lovers|rimming|frottage|grinding|domination|dominating|submissive|bondage|bdsm|mating|breeding|copulation');
  var SETTING_WORDS = set('background|indoors|indoor|inside|outdoors|outdoor|outside|forest|woods|jungle|swamp|city|cityscape|town|village|street|alley|road|bridge|building|buildings|house|home|room|bedroom|bathroom|kitchen|classroom|school|office|library|gym|lockerroom|locker|shower|sauna|onsen|pool|beach|ocean|sea|lake|river|waterfall|water|underwater|mountain|mountains|hill|hills|cliff|cave|desert|field|meadow|grass|garden|park|castle|palace|dungeon|prison|ruins|temple|shrine|church|cathedral|tavern|inn|bar|pub|restaurant|cafe|market|shop|store|stage|arena|battlefield|colosseum|throne|space|planet|galaxy|sky|clouds|cloud|night|day|daytime|nighttime|sunset|sunrise|dusk|dawn|twilight|evening|morning|noon|afternoon|rain|rainy|raining|snow|snowing|snowy|fog|foggy|mist|misty|storm|thunderstorm|winter|summer|autumn|landscape|scenery|nature|bed|couch|sofa|chair|table|desk|window|wall|floor|ground|tree|trees|flowers|flower|petals|leaves|stars|starry|moon|sun|fire|flames|campfire|lava|ice|ship|boat|train|car|spaceship|tent|camp|farm|barn|stable|ranch|bath|bathtub|dojo|laboratory|lab|hospital|graveyard|cemetery|scene|environment|location|world|universe|interior|exterior|backdrop|horizon|skyline|rooftop|balcony|porch|courtyard|plaza|harbor|dock|pier|island|volcano|canyon|valley|tundra|savanna|glacier|oasis|farmland|countryside|suburb|downtown|subway|station|airport|hallway|corridor|staircase|stairs|basement|attic|garage|warehouse|factory|workshop|forge|mine|tower|bedsheet|pillow|carpet|rug|curtains|bookshelf|fireplace|lamp|candle|candles|mirror');
  var CAMERA_EXACT = set('close-up|closeup|close up|extreme close-up|portrait|profile|upper body|lower body|full body|cowboy shot|head shot|headshot|wide shot|very wide shot|medium shot|long shot|establishing shot|from above|from below|from behind|from side|from the side|from front|from outside|from inside|dutch angle|low angle|high angle|bird\'s-eye view|birds eye view|bird\'s eye view|worm\'s-eye view|aerial view|overhead view|side view|front view|back view|rear view|three-quarter view|dynamic angle|fisheye|panorama|panoramic|foreshortening|pov|first-person view|first person view|depth of field|dof|bokeh|blurry|blurry background|blurry foreground|motion blur|chromatic aberration|film grain|vignette|vignetting|wide-angle|wide angle|telephoto|centered|symmetry|rule of thirds|cropped|out of frame|feet out of frame|head out of frame|multiple views|split screen|selfie|mirror selfie|zoom|zoomed in|zoomed out|looking at camera|pov hands|pov crotch|straight-on|tilted frame');
  var CAMERA_WORDS = set('shot|angle|view|perspective|framing|composition|lens|camera|close-up|closeup|portrait|bokeh|blur|blurry|focus|fisheye|panorama|foreshortening|pov|zoom|cropped|telephoto|macro|photo-shoot');
  var LIGHTING_WORDS = set('lighting|light|lights|lit|backlighting|backlit|sunlight|moonlight|candlelight|firelight|lamplight|starlight|sunbeam|sunbeams|rays|shadow|shadows|shade|glow|glowing|bloom|flare|chiaroscuro|silhouette|dark|darkness|dim|bright|contrast|neon|illumination|illuminated|reflection|reflections|sparkle|sparkles|sparkling|luminous|radiant|twinkling|gloomy|shaded|spotlight|rimlight|volumetric|caustics');
  var LIGHTING_EXACT = set('rim light|rim lighting|volumetric lighting|cinematic lighting|dramatic lighting|soft lighting|studio lighting|natural light|natural lighting|god rays|light rays|crepuscular rays|hard shadows|lens flare|high contrast|low key|high key|neon lights|golden hour|blue hour|ambient occlusion|subsurface scattering|ray tracing|global illumination|dappled sunlight|light particles|soft light|hard light|warm light|cold light|colored light|backlight');
  var STYLE_EXACT = set('anime|anime style|manga|cartoon|comic|comic style|realistic|realism|photorealistic|photorealism|hyperrealistic|semi-realistic|photo|photograph|photography|raw photo|3d|2d|cgi|render|octane render|unreal engine|blender|painting|oil painting|watercolor|watercolour|acrylic|gouache|pastel|digital painting|digital art|illustration|concept art|sketch|lineart|line art|ink|inking|monochrome|greyscale|grayscale|sepia|cel shading|cel-shaded|flat color|flat colors|pixel art|low poly|vector|traditional media|traditional art|impressionism|expressionism|art nouveau|art deco|ukiyo-e|baroque|renaissance|surreal|surrealism|minimalism|minimalist|abstract|pop art|graffiti|fantasy art|dark fantasy|fantasy|sci-fi|science fiction|cyberpunk|steampunk|gothic|retro|vintage|vaporwave|synthwave|chibi|kawaii|toon|disney|pixar|ghibli|studio ghibli|official art|game cg|screencap|anime screencap|anime coloring|colorful|vibrant colors|vibrant|muted colors|pastel colors|warm colors|cool colors|limited palette|high saturation|painterly|poster|wallpaper|album cover|magazine cover|cinematic|film still|movie still|studio photo|editorial|fashion photography|polaroid|analog|film|35mm film|kodak|fujifilm');
  var STYLE_WORDS = set('style|styled|anime|manga|cartoon|comic|realistic|photorealistic|painting|watercolor|sketch|lineart|illustration|render|monochrome|greyscale|grayscale|sepia|art|artwork|drawing|drawn|painted|aesthetic|palette|shading|colors|colours|coloring|colouring|cel');
  var META_EXACT = set('signature|watermark|text|english text|japanese text|artist name|web address|speech bubble|username|patreon username|twitter username|logo|border|letterboxed|jpeg artifacts|commentary|translated|dated|copyright name|character name|sound effects|onomatopoeia|caption|subtitles|censored|uncensored|mosaic censoring|bar censor|commission|patreon reward|paid reward|variant set|comic panel|4koma|lowres|scan|third-party edit|photoshop \\(medium\\)|absurdres');
  var SUBJECT_COUNT_RE = /^(\d+\+?|multiple|many|several|two|three|four|five)\s?(girls?|boys?|others?|futas?|males?|females?|women|men|people|persons|characters)$/;
  var DATE_RE = /^(year\s?)?(19|20)\d\d(s)?$/;
  var SCORE_RE = /^score[\s_]?\d(\s?up)?$/;
  var SOURCE_RE = /^source[\s_](anime|cartoon|furry|pony|comic|manga|real|photo|3d|western|game)/;
  var RATING_RE = /^rating[\s_:]/;
  var ARTIST_RE = /^(by|art by|artist)\s+(?!(the|a|an|his|her|their|my|your|its)\b)\S|^@\S|^artist:\s*\S/;
  var LIMB_POSE_RE = /^(hand|hands|arm|arms|finger|fingers|paw|paws|leg|legs|foot|feet|knee|knees|head|tail) (on|in|behind|over|under|between|around|up|down|to|raised|lifted|out|together|apart)\b/;
  var LENS_RE = /^\d+\s?mm(\s(lens|photo|photograph))?$|^f\/\d/;
  function classifierKey(seg) {
    if (!seg) return '';
    var base = seg.kind === 'weighted' ? seg.base : seg.text;
    if (seg.kind === 'schedule' || seg.kind === 'group') {
      base = String(seg.text || '').replace(/^[[{(]+|[\]})]+$/g, '').split(/[:|]/).filter(function (part) {
        return part.trim() && !/^\s*[-+]?\d*\.?\d+\s*$/.test(part);
      }).join(' ');
    }
    return String(base || '').replace(/\\([()[\]{}])/g, '$1').replace(/_/g, ' ').replace(/\s+/g, ' ').trim().toLowerCase();
  }
  var tokenize = function tokenize(key) {
    return key.replace(/[()[\]{}"«»“”]/g, ' ').split(/[\s,;]+/).map(function (w) {
      return w.replace(/^[.'!?]+|[.'!?:]+$/g, '');
    }).filter(Boolean);
  };
  var WORD_TABLES = [['species', SPECIES_WORDS], ['body', BODY_HEADS], ['clothing', CLOTHING_WORDS], ['expression', EXPRESSION_WORDS], ['interaction', INTERACTION_WORDS], ['pose', POSE_WORDS], ['setting', SETTING_WORDS], ['camera', CAMERA_WORDS], ['lighting', LIGHTING_WORDS], ['style', STYLE_WORDS], ['subject', SUBJECT_WORDS], ['body', BODY_WORDS]];
  function wordCategory(word) {
    var _iterator2 = _createForOfIteratorHelper(WORD_TABLES),
      _step2;
    try {
      for (_iterator2.s(); !(_step2 = _iterator2.n()).done;) {
        var _step2$value = _slicedToArray(_step2.value, 2),
          category = _step2$value[0],
          table = _step2$value[1];
        if (table.has(word)) return category;
      }
    } catch (err) {
      _iterator2.e(err);
    } finally {
      _iterator2.f();
    }
    if (/(style)$/.test(word) && word !== 'hairstyle') return 'style';
    return null;
  }
  function qualitySub(key) {
    if (SCORE_RE.test(key)) return 'score';
    if (SOURCE_RE.test(key)) return 'source';
    if (RATING_RE.test(key) || RATING_EXACT.has(key)) return 'rating';
    if (AESTHETIC_EXACT.has(key)) return 'aesthetic';
    if (DATE_EXACT.has(key) || DATE_RE.test(key)) return 'date';
    if (RESOLUTION_EXACT.has(key)) return 'resolution';
    if (QUALITY_EXACT.has(key)) return 'quality';
    if (/^embedding:/.test(key)) return 'embedding';
    return null;
  }
  function voteCategory(key, rankOf) {
    var words = tokenize(key);
    var counts = new Map();
    var add = function add(cat, w) {
      return counts.set(cat, (counts.get(cat) || 0) + w);
    };
    for (var n = 3; n >= 2; n--) {
      for (var i = 0; i + n <= words.length; i++) {
        var phrase = words.slice(i, i + n).join(' ');
        var cat = exactCategory(phrase);
        if (cat) add(cat, 2);
      }
    }
    words.forEach(function (w) {
      if (/^(a|an|the|and|with|of|in|on|at|by|for|to|from|his|her|their|its|is|are|wearing)$/.test(w)) {
        if (w === 'wearing') add('clothing', 1);
        return;
      }
      var cat = wordCategory(w) || (SUBJECT_COUNT_RE.test(w) ? 'subject' : null);
      if (cat) add(cat, cat === 'subject' || cat === 'species' ? 2 : 1);
    });
    var best = null;
    var bestScore = 0;
    counts.forEach(function (score, cat) {
      if (score > bestScore || score === bestScore && best && rankOf(cat) < rankOf(best)) {
        best = cat;
        bestScore = score;
      }
    });
    return best;
  }
  function exactCategory(key) {
    if (qualitySub(key)) return 'quality';
    if (SUBJECT_EXACT.has(key) || SUBJECT_COUNT_RE.test(key)) return 'subject';
    if (EXPRESSION_EXACT.has(key)) return 'expression';
    if (POSE_EXACT.has(key)) return 'pose';
    if (INTERACTION_EXACT.has(key)) return 'interaction';
    if (CAMERA_EXACT.has(key) || LENS_RE.test(key)) return 'camera';
    if (LIGHTING_EXACT.has(key)) return 'lighting';
    if (STYLE_EXACT.has(key)) return 'style';
    if (META_EXACT.has(key)) return 'meta';
    return null;
  }
  function classifyPromptSegment(seg) {
    var _entry;
    var ctx = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : {};
    if (!seg) return {
      category: 'unknown',
      sub: null,
      source: 'none'
    };
    if (seg.kind === 'keyword') return {
      category: 'keyword',
      sub: null,
      source: 'syntax'
    };
    if (seg.kind === 'lora' || seg.kind === 'extra') return {
      category: 'lora',
      sub: null,
      source: 'syntax'
    };
    var key = classifierKey(seg);
    if (!key) return {
      category: 'unknown',
      sub: null,
      source: 'none'
    };
    if (/^embedding:/.test(key)) return {
      category: 'quality',
      sub: 'embedding',
      source: 'rule'
    };
    var sub = qualitySub(key);
    if (sub) return {
      category: 'quality',
      sub: sub,
      source: 'rule'
    };
    var triggers = ctx.triggerWords;
    if (triggers && typeof triggers.has === 'function' && triggers.has(key)) {
      return {
        category: 'trigger',
        sub: null,
        source: 'rule'
      };
    }
    if (ARTIST_RE.test(key) && !/\bstyle\b/.test(key)) return {
      category: 'artist',
      sub: null,
      source: 'rule'
    };
    var exact = exactCategory(key);
    if (exact) return {
      category: exact,
      sub: null,
      source: 'rule'
    };
    var entry = null;
    if (typeof ctx.lookup === 'function') {
      try {
        entry = ctx.lookup(key);
      } catch (_unused) {
        entry = null;
      }
    }
    var dictCat = (_entry = entry) === null || _entry === void 0 ? void 0 : _entry.category;
    if (dictCat === 'character' || dictCat === 'copyright' || dictCat === 'artist') {
      return {
        category: dictCat,
        sub: null,
        source: 'dict'
      };
    }
    var words = tokenize(key);
    var rankOf = rankFunction(ctx.family || 'illustrious');
    if (words.length >= 4) {
      var voted = voteCategory(key, rankOf);
      return voted ? {
        category: voted,
        sub: null,
        source: 'vote'
      } : {
        category: 'unknown',
        sub: null,
        source: 'none'
      };
    }
    var first = words[0] || '';
    var last = words[words.length - 1] || '';
    if (/^(looking|facing|glancing|staring|gazing)$/.test(first)) return {
      category: 'expression',
      sub: null,
      source: 'rule'
    };
    if (/^(hugging|kissing|embracing|cuddling|groping|licking|biting|sucking|grabbing|touching|petting|spanking|tickling|penetrating|straddling|riding|carrying)$/.test(first) && words.length > 1 && (/\b(another|other|partner|each)\b/.test(key) || first !== 'riding')) {
      return {
        category: 'interaction',
        sub: null,
        source: 'rule'
      };
    }
    if (first === 'holding' || first === 'wielding') return {
      category: 'pose',
      sub: null,
      source: 'rule'
    };
    if (LIMB_POSE_RE.test(key)) return {
      category: 'pose',
      sub: null,
      source: 'rule'
    };
    if (first === 'from' && words.length > 1) return {
      category: 'camera',
      sub: null,
      source: 'rule'
    };
    if (/\b(another|other's|each other)\b/.test(key)) return {
      category: 'interaction',
      sub: null,
      source: 'rule'
    };
    if (/\bbackground$/.test(key)) return {
      category: 'setting',
      sub: null,
      source: 'rule'
    };
    if (/\bstyle$/.test(key) || /^style\b/.test(key)) return {
      category: 'style',
      sub: null,
      source: 'rule'
    };
    if (/^(boy|girl|man|woman|guy|male|female|boys|girls|men|women|males|females)$/.test(last) && words.length > 1) {
      if (words.slice(0, -1).some(function (w) {
        return SPECIES_WORDS.has(w);
      })) return {
        category: 'species',
        sub: null,
        source: 'rule'
      };
      if (/^(male|female|males|females)$/.test(last)) return {
        category: 'body',
        sub: null,
        source: 'rule'
      };
    }
    var prep = /^(.+?) (on|over|across|around|under|in|with) (.+)$/.exec(key);
    if (prep) {
      var leftWords = tokenize(prep[1]);
      var leftCat = wordCategory(leftWords[leftWords.length - 1] || '');
      if (leftCat) return {
        category: leftCat,
        sub: null,
        source: 'rule'
      };
    }
    var headCat = wordCategory(last);
    if (headCat) return {
      category: headCat,
      sub: null,
      source: 'rule'
    };
    for (var i = words.length - 2; i >= 0; i--) {
      var cat = wordCategory(words[i]);
      if (cat) return {
        category: cat,
        sub: null,
        source: 'rule'
      };
    }
    if (/^[a-z]{3,}ing$/.test(first) && !/^(lighting|clothing|evening|morning|ceiling|building|painting|drawing)$/.test(first)) {
      return {
        category: 'pose',
        sub: null,
        source: 'rule'
      };
    }
    if (dictCat === 'meta') return {
      category: 'meta',
      sub: null,
      source: 'dict'
    };
    if (/\S \([^)]+\)$/.test(key)) return {
      category: 'character',
      sub: null,
      source: 'rule'
    };
    return {
      category: 'unknown',
      sub: null,
      source: 'none'
    };
  }
  function rankFunction(family) {
    if (family === 'natural') {
      return function (cat) {
        return cat in NATURAL_RANK ? NATURAL_RANK[cat] : 50;
      };
    }
    var order = ORDERS[family] || ORDERS.illustrious;
    return function (cat) {
      var i = order.indexOf(cat);
      return i >= 0 ? i : 50;
    };
  }
  function subRankFunction(family) {
    var list = family === 'pony' ? QUALITY_SUB.pony : QUALITY_SUB.default;
    return function (sub) {
      var i = list.indexOf(sub || 'quality');
      return i >= 0 ? i : list.length;
    };
  }
  function classifyAll(segments) {
    var ctx = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : {};
    var labels = ctx.labels || null;
    return segments.map(function (seg, i) {
      var own = classifyPromptSegment(seg, ctx);
      if (own.category === 'unknown' && labels && labels[i] && PROMPT_CATEGORIES.includes(labels[i]) && labels[i] !== 'unknown') {
        return {
          category: labels[i],
          sub: null,
          source: 'ai'
        };
      }
      return own;
    });
  }
  function effectiveCategories(indices, cats) {
    var out = new Map();
    var prev = null;
    var pending = [];
    indices.forEach(function (i) {
      var c = cats[i].category;
      if (c === 'unknown') {
        if (prev) out.set(i, _objectSpread(_objectSpread({}, prev), {}, {
          glued: true
        }));else pending.push(i);
        return;
      }
      prev = cats[i];
      out.set(i, cats[i]);
      while (pending.length) out.set(pending.shift(), _objectSpread(_objectSpread({}, cats[i]), {}, {
        glued: true
      }));
    });
    pending.forEach(function (i) {
      return out.set(i, {
        category: 'unknown',
        sub: null,
        glued: true
      });
    });
    return out;
  }
  function splitBlocks(segments) {
    var blocks = [[]];
    segments.forEach(function (seg, i) {
      if (seg.kind === 'keyword') blocks.push([]);else blocks[blocks.length - 1].push(i);
    });
    return blocks.filter(function (b) {
      return b.length;
    });
  }
  function rebuildWithPermutation(text, segments, slotToSource) {
    var out = '';
    var cursor = 0;
    segments.forEach(function (seg, idx) {
      out += text.slice(cursor, seg.start);
      out += segments[slotToSource[idx]].text;
      cursor = seg.end;
    });
    out += text.slice(cursor);
    return out;
  }
  function sortBlock(block, catOf, rankOf, subRankOf) {
    var groups = [];
    var pending = [];
    block.forEach(function (i, pos) {
      var c = catOf(i);
      if (!c || c.category === 'unknown') {
        if (groups.length) groups[groups.length - 1].members.push(i);else pending.push(i);
        return;
      }
      groups.push({
        key: c,
        members: [].concat(_toConsumableArray(pending), [i]),
        pos: pos
      });
      pending = [];
    });
    if (pending.length) groups.push({
      key: {
        category: 'unknown',
        sub: null
      },
      members: pending,
      pos: -1
    });
    groups.sort(function (a, b) {
      var ra = rankOf(a.key.category);
      var rb = rankOf(b.key.category);
      if (ra !== rb) return ra - rb;
      if (a.key.category === 'quality' && b.key.category === 'quality') {
        var sa = subRankOf(a.key.sub);
        var sb = subRankOf(b.key.sub);
        if (sa !== sb) return sa - sb;
      }
      return a.pos - b.pos;
    });
    return groups.flatMap(function (g) {
      return g.members;
    });
  }
  var TERMINATOR_RE = /^[ \t]*[.!?。]/;
  function sentenceUnits(text, segments, block) {
    var units = [];
    var current = [];
    block.forEach(function (i, pos) {
      current.push(i);
      var seg = segments[i];
      var nextSeg = pos + 1 < block.length ? segments[block[pos + 1]] : null;
      var gap = text.slice(seg.end, nextSeg ? nextSeg.start : text.length);
      if (!nextSeg || TERMINATOR_RE.test(gap) || gap.includes('\n')) {
        units.push(current);
        current = [];
      }
    });
    if (current.length) units.push(current);
    return units;
  }
  function organizeNaturalSentences(text, segments, block, units, cats, ctx, rankOf) {
    var unitCats = units.map(function (unit) {
      var first = segments[unit[0]];
      var last = segments[unit[unit.length - 1]];
      var body = text.slice(first.start, last.end);
      var labelled = unit.map(function (i) {
        return cats[i];
      }).filter(function (c) {
        return c.source === 'ai';
      });
      if (labelled.length) {
        return {
          category: labelled[0].category,
          sub: null
        };
      }
      var own = unit.length === 1 ? cats[unit[0]] : {
        category: voteCategory(body.toLowerCase(), rankOf) || 'unknown'
      };
      return {
        category: own.category || 'unknown',
        sub: own.sub || null
      };
    });
    var order = sortBlock(units.map(function (_, k) {
      return k;
    }), function (k) {
      return unitCats[k];
    }, rankOf, function () {
      return 0;
    });
    if (order.every(function (k, pos) {
      return k === pos;
    })) return null;
    var pieces = units.map(function (unit) {
      var first = segments[unit[0]];
      var last = segments[unit[unit.length - 1]];
      var end = last.end;
      var m = TERMINATOR_RE.exec(text.slice(end));
      if (m) end += m[0].length;
      return {
        start: first.start,
        end: end,
        text: text.slice(first.start, end),
        terminated: !!m
      };
    });
    var out = text.slice(0, pieces[0].start);
    order.forEach(function (k, pos) {
      var piece = pieces[k];
      var slot = pieces[pos];
      var gapEnd = pos + 1 < pieces.length ? pieces[pos + 1].start : text.length;
      var gap = text.slice(slot.end, gapEnd);
      var body = piece.text;
      if (!piece.terminated && pos + 1 < pieces.length && !gap.includes('\n')) body += '.';
      out += body + gap;
    });
    return {
      text: out,
      unitOrder: order,
      units: units
    };
  }
  function organizePrompt(text) {
    var options = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : {};
    var src = typeof text === 'string' ? text : '';
    var family = ORDER_FAMILIES.includes(options.family) ? options.family : 'illustrious';
    var ctx = _objectSpread(_objectSpread({}, options), {}, {
      family: family
    });
    var segments = segmentPrompt(src);
    var cats = classifyAll(segments, ctx);
    var rankOf = rankFunction(family);
    var subRankOf = subRankFunction(family);
    var unknown = cats.map(function (c, i) {
      return c.category === 'unknown' ? i : -1;
    }).filter(function (i) {
      return i >= 0;
    });
    var slotToSource = segments.map(function (_, i) {
      return i;
    });
    var out = src;
    var mode = 'tags';
    var blocks = splitBlocks(segments);
    var naturalDone = false;
    if (family === 'natural' && blocks.length) {
      var tagLike = segments.every(function (s) {
        return s.kind !== 'tag' || s.words <= 3;
      });
      if (!tagLike) {
        mode = 'sentences';
        naturalDone = true;
        var single = blocks.length === 1 && blocks[0].length === segments.length;
        var units = single ? sentenceUnits(src, segments, blocks[0]) : null;
        if (units && units.length > 1) {
          var r = organizeNaturalSentences(src, segments, blocks[0], units, cats, ctx, rankOf);
          if (r) {
            out = r.text;
            r.unitOrder.flatMap(function (k) {
              return units[k];
            }).forEach(function (sourceIndex, pos) {
              slotToSource[pos] = sourceIndex;
            });
          }
        }
      }
    }
    if (!naturalDone) {
      blocks.forEach(function (block) {
        var order = sortBlock(block, function (i) {
          return cats[i];
        }, rankOf, subRankOf);
        block.forEach(function (slot, pos) {
          slotToSource[slot] = order[pos];
        });
      });
      out = rebuildWithPermutation(src, segments, slotToSource);
    }
    var removed = [];
    if (options.dedupe) {
      var _r = removeDuplicates(out);
      out = _r.text;
      removed = _r.removed;
    }
    var order = slotToSource.filter(function (i) {
      return segments[i].kind !== 'keyword';
    });
    var moved = slotToSource.reduce(function (n, source, slot) {
      return n + (source !== slot ? 1 : 0);
    }, 0);
    return {
      text: out,
      changed: out !== src,
      moved: moved,
      mode: mode,
      family: family,
      segments: segments,
      categories: cats,
      unknown: unknown,
      order: order,
      slots: slotToSource,
      removed: removed
    };
  }
  function removeDuplicates(text) {
    var out = typeof text === 'string' ? text : '';
    var removed = [];
    var segs = segmentPrompt(out);
    var seen = new Set();
    var drop = [];
    segs.forEach(function (seg, i) {
      if (seg.kind === 'keyword') return;
      var key = normalizeKey(seg);
      if (!key) return;
      if (seen.has(key)) drop.push(i);else seen.add(key);
    });
    for (var k = drop.length - 1; k >= 0; k--) {
      var segsNow = segmentPrompt(out);
      var i = drop[k];
      removed.unshift(segsNow[i].text);
      var seg = segsNow[i];
      var prev = segsNow[i - 1];
      var next = segsNow[i + 1];
      if (prev) out = out.slice(0, prev.end) + out.slice(seg.end);else if (next) out = out.slice(0, seg.start) + out.slice(next.start);else out = out.slice(0, seg.start) + out.slice(seg.end);
    }
    return {
      text: out,
      removed: removed
    };
  }
  function isPermutationOfSegments(before, after) {
    var a = segmentPrompt(String(before || '')).map(function (s) {
      return s.text;
    });
    var b = segmentPrompt(String(after || '')).map(function (s) {
      return s.text;
    });
    if (a.length !== b.length) return false;
    var counts = new Map();
    a.forEach(function (x) {
      return counts.set(x, (counts.get(x) || 0) + 1);
    });
    var _iterator3 = _createForOfIteratorHelper(b),
      _step3;
    try {
      for (_iterator3.s(); !(_step3 = _iterator3.n()).done;) {
        var x = _step3.value;
        var n = counts.get(x);
        if (!n) return false;
        counts.set(x, n - 1);
      }
    } catch (err) {
      _iterator3.e(err);
    } finally {
      _iterator3.f();
    }
    return true;
  }
  function blockOf(segments, index) {
    var start = 0;
    for (var i = 0; i < segments.length; i++) {
      if (segments[i].kind === 'keyword') {
        if (i >= index) break;
        start = i + 1;
      }
    }
    var end = segments.length - 1;
    for (var _i3 = Math.max(index, start); _i3 < segments.length; _i3++) {
      if (segments[_i3].kind === 'keyword') {
        end = _i3 - 1;
        break;
      }
    }
    var out = [];
    for (var _i4 = start; _i4 <= end; _i4++) out.push(_i4);
    return out;
  }
  function logicalSlot(block, cats, cat, rankOf, subRankOf) {
    var skip = arguments.length > 5 && arguments[5] !== undefined ? arguments[5] : -1;
    var others = block.filter(function (i) {
      return i !== skip;
    });
    if (!others.length) return null;
    var eff = effectiveCategories(others, cats);
    var keyOf = function keyOf(c) {
      return rankOf(c.category) * 100 + (c.category === 'quality' ? subRankOf(c.sub) : 0);
    };
    var mine = keyOf(cat);
    var keys = others.map(function (i) {
      return keyOf(eff.get(i));
    });
    var later = 0;
    var earlier = keys.filter(function (k) {
      return k < mine;
    }).length;
    var best = {
      cost: later + earlier,
      pos: 0
    };
    keys.forEach(function (k, idx) {
      if (k > mine) later += 1;
      if (k < mine) earlier -= 1;
      var cost = later + earlier;
      if (cost <= best.cost) best = {
        cost: cost,
        pos: idx + 1
      };
    });
    return best.pos > 0 ? {
      after: others[best.pos - 1]
    } : {
      before: others[0]
    };
  }
  function insertTagLogically(text, piece) {
    var options = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : {};
    var src = typeof text === 'string' ? text : '';
    var clean = cleanInsert(piece);
    if (!clean) return null;
    var segments = segmentPrompt(src);
    if (!segments.length) return {
      text: clean,
      start: 0,
      end: clean.length
    };
    var pieceSeg = segmentPrompt(clean);
    var pieceKey = pieceSeg.length === 1 ? normalizeKey(pieceSeg[0]) : normalizeKey(clean);
    var existing = segments.findIndex(function (s) {
      return pieceKey && normalizeKey(s) === pieceKey;
    });
    if (existing >= 0) {
      return {
        text: src,
        start: segments[existing].start,
        end: segments[existing].end,
        existed: true
      };
    }
    var family = ORDER_FAMILIES.includes(options.family) ? options.family : 'illustrious';
    var ctx = _objectSpread(_objectSpread({}, options), {}, {
      family: family
    });
    var cat = options.category ? {
      category: options.category,
      sub: null
    } : pieceSeg.length === 1 ? classifyPromptSegment(pieceSeg[0], ctx) : {
      category: 'unknown',
      sub: null
    };
    var naturalSentences = family === 'natural' && segments.some(function (s) {
      return s.sentence;
    });
    if (cat.category === 'unknown' || naturalSentences) return _objectSpread(_objectSpread({}, appendSegment(src, clean)), {}, {
      category: cat.category
    });
    var cats = classifyAll(segments, ctx);
    var block = blockOf(segments, 0);
    var slot = logicalSlot(block, cats, cat, rankFunction(family), subRankFunction(family));
    if (!slot) return _objectSpread(_objectSpread({}, appendSegment(src, clean)), {}, {
      category: cat.category
    });
    if (slot.after !== undefined) {
      return _objectSpread(_objectSpread({}, insertAfterSegment(src, segments, slot.after, clean)), {}, {
        category: cat.category
      });
    }
    var s = segments[slot.before];
    var out = "".concat(src.slice(0, s.start)).concat(clean, ", ").concat(src.slice(s.start));
    return {
      text: out,
      start: s.start,
      end: s.start + clean.length,
      category: cat.category
    };
  }
  function logicalMoveTarget(text, segments, index) {
    var options = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : {};
    var segs = segments || segmentPrompt(text);
    var seg = segs[index];
    if (!seg || seg.kind === 'keyword') return null;
    var family = ORDER_FAMILIES.includes(options.family) ? options.family : 'illustrious';
    var ctx = _objectSpread(_objectSpread({}, options), {}, {
      family: family
    });
    var cats = classifyAll(segs, ctx);
    var cat = cats[index];
    if (cat.category === 'unknown') return null;
    var block = blockOf(segs, index);
    var slot = logicalSlot(block, cats, cat, rankFunction(family), subRankFunction(family), index);
    if (!slot) return null;
    var to;
    if (slot.after !== undefined) to = slot.after < index ? slot.after + 1 : slot.after;else to = slot.before < index ? slot.before : slot.before - 1;
    return to === index ? null : to;
  }
  function moveSegmentLogically(text, segments, index) {
    var options = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : {};
    var segs = segments || segmentPrompt(text);
    var to = logicalMoveTarget(text, segs, index, options);
    if (to === null) return null;
    return moveSegment(text, segs, index, to);
  }
  function categoryStepTarget(text, segments, index, dir) {
    var options = arguments.length > 4 && arguments[4] !== undefined ? arguments[4] : {};
    var segs = segments || segmentPrompt(text);
    if (!segs[index] || segs[index].kind === 'keyword') return null;
    var family = ORDER_FAMILIES.includes(options.family) ? options.family : 'illustrious';
    var cats = classifyAll(segs, _objectSpread(_objectSpread({}, options), {}, {
      family: family
    }));
    var block = blockOf(segs, index);
    var eff = effectiveCategories(block, cats);
    var catAt = function catAt(i) {
      var _eff$get;
      return (_eff$get = eff.get(i)) === null || _eff$get === void 0 ? void 0 : _eff$get.category;
    };
    var own = catAt(index);
    var lo = block[0];
    var hi = block[block.length - 1];
    var j = index + dir;
    while (j >= lo && j <= hi && catAt(j) === own) j += dir;
    if (j < lo || j > hi) {
      var edge = dir < 0 ? lo : hi;
      return edge === index ? null : edge;
    }
    var other = catAt(j);
    while (j >= lo && j <= hi && catAt(j) === other) j += dir;
    var to = j - dir;
    return to === index ? null : to;
  }
  function orderPreviewCategories(family) {
    if (family === 'natural') {
      return ['subject', 'body', 'pose', 'setting', 'camera', 'lighting', 'style', 'quality'];
    }
    return (ORDERS[family] || ORDERS.illustrious).filter(function (c) {
      return c !== 'lora';
    });
  }
  var CATEGORY_COLORS = {
    general: '#378ADD',
    artist: '#D4537E',
    copyright: '#7F77DD',
    character: '#639922',
    meta: '#888780',
    custom: '#9aa0a6'
  };
  function categoryColor(category) {
    return CATEGORY_COLORS[category] || CATEGORY_COLORS.general;
  }
  var SUGGEST_MIN_COUNT = 200;
  function boundedLevenshtein(a, b, ceil) {
    var m = a.length;
    var n = b.length;
    if (Math.abs(m - n) > ceil) return ceil + 1;
    if (m === 0) return n;
    if (n === 0) return m;
    var prevRow = new Array(n + 1);
    for (var j = 0; j <= n; j++) prevRow[j] = j;
    for (var i = 1; i <= m; i++) {
      var curr = new Array(n + 1);
      curr[0] = i;
      var rowMin = curr[0];
      var ca = a.charCodeAt(i - 1);
      for (var _j = 1; _j <= n; _j++) {
        var cost = ca === b.charCodeAt(_j - 1) ? 0 : 1;
        curr[_j] = Math.min(prevRow[_j] + 1, curr[_j - 1] + 1, prevRow[_j - 1] + cost);
        if (curr[_j] < rowMin) rowMin = curr[_j];
      }
      if (rowMin > ceil) return ceil + 1;
      prevRow = curr;
    }
    return prevRow[n];
  }
  function buildIndex(tags) {
    var known = new Map();
    var byFirst = new Map();
    if (!Array.isArray(tags)) return {
      known: known,
      byFirst: byFirst
    };
    for (var i = 0; i < tags.length; i++) {
      var entry = tags[i];
      var raw = entry && entry.tag;
      if (!raw) continue;
      var t = String(raw).toLowerCase().replace(/_/g, ' ').trim();
      if (!t) continue;
      if (known.has(t)) continue;
      known.set(t, entry.category || 'general');
      var count = entry.count || 0;
      if (count < SUGGEST_MIN_COUNT) continue;
      var key = t.slice(0, 2);
      var bucket = byFirst.get(key);
      if (!bucket) {
        bucket = [];
        byFirst.set(key, bucket);
      }
      bucket.push({
        t: t,
        len: t.length,
        count: count
      });
    }
    return {
      known: known,
      byFirst: byFirst
    };
  }
  function samePlural(a, b) {
    var _ref3 = a.length <= b.length ? [a, b] : [b, a],
      _ref4 = _slicedToArray(_ref3, 2),
      short = _ref4[0],
      long = _ref4[1];
    if (long === "".concat(short, "s")) return true;
    if (long === "".concat(short, "es")) return true;
    if (short.endsWith('y') && long === "".concat(short.slice(0, -1), "ies")) return true;
    return false;
  }
  function isInflectionOfKnown(word, known) {
    if (word.length < 4) return false;
    var stems = [];
    var push = function push(s) {
      if (s && s.length >= 3) stems.push(s);
    };
    if (word.endsWith('ies')) push("".concat(word.slice(0, -3), "y"));
    if (word.endsWith('es')) push(word.slice(0, -2));
    if (word.endsWith('s')) push(word.slice(0, -1));
    if (word.endsWith('ing')) {
      push(word.slice(0, -3));
      push("".concat(word.slice(0, -3), "e"));
    }
    if (word.endsWith('ed')) {
      push(word.slice(0, -2));
      push(word.slice(0, -1));
    }
    if (word.endsWith('er')) {
      push(word.slice(0, -2));
      push(word.slice(0, -1));
    }
    if (word.endsWith('ly')) push(word.slice(0, -2));
    var _iterator4 = _createForOfIteratorHelper(stems.slice()),
      _step4;
    try {
      for (_iterator4.s(); !(_step4 = _iterator4.n()).done;) {
        var _s = _step4.value;
        if (_s.length >= 4 && _s[_s.length - 1] === _s[_s.length - 2]) push(_s.slice(0, -1));
      }
    } catch (err) {
      _iterator4.e(err);
    } finally {
      _iterator4.f();
    }
    for (var _i5 = 0, _stems = stems; _i5 < _stems.length; _i5++) {
      var s = _stems[_i5];
      if (known.has(s)) return true;
    }
    return false;
  }
  function isTypoOf(word, cand) {
    if (word === cand) return false;
    if (samePlural(word, cand)) return false;
    if (word.length < 5) return false;
    var ceil = word.length >= 9 ? 2 : 1;
    if (Math.abs(word.length - cand.length) > ceil) return false;
    if (word[0] !== cand[0] || word[1] !== cand[1]) return false;
    var d = boundedLevenshtein(word, cand, ceil);
    return d >= 1 && d <= ceil;
  }
  function normalizeSegment(core) {
    return core.toLowerCase().replace(/^[([{]+/, '').replace(/[)\]}]+$/, '').replace(/:\s*-?\d+(\.\d+)?$/, '').replace(/\\/g, '').trim();
  }
  var COMMON_WORDS = new Set(['lean', 'slim', 'slender', 'curvy', 'chubby', 'stocky', 'lanky', 'petite', 'tall', 'short', 'gentle', 'fierce', 'serene', 'calm', 'moody', 'vibrant', 'muted', 'pale', 'rich', 'deep', 'glossy', 'matte', 'velvet', 'leather', 'denim', 'cotton', 'satin', 'silk', 'woolen', 'linen', 'golden', 'silver', 'bronze', 'copper', 'crimson', 'scarlet', 'azure', 'teal', 'amber', 'ivory', 'beautiful', 'gorgeous', 'elegant', 'graceful', 'majestic', 'ancient', 'modern', 'mystical', 'magical', 'ethereal', 'radiant', 'luminous', 'cozy', 'warm', 'cool', 'soft', 'sharp', 'smooth', 'rough', 'shiny', 'misty', 'foggy', 'snowy', 'rainy', 'sunny', 'stormy', 'dreamy', 'gloomy', 'happy', 'sad', 'angry', 'tired', 'young', 'older', 'small', 'large', 'huge', 'tiny', 'wearing', 'holding', 'standing', 'sitting', 'lying', 'running', 'looking', 'facing', 'background', 'foreground', 'detailed', 'realistic', 'simple', 'complex', 'colorful', 'bright', 'dark', 'light']);
  function suggestTag(phrase, index) {
    if (phrase.length < 5) return null;
    var bucket = index.byFirst.get(phrase.slice(0, 2));
    if (!bucket) return null;
    var words = phrase.split(/\s+/);
    var MIN_COUNT = SUGGEST_MIN_COUNT;
    var best = null;
    var bestScore = Infinity;
    for (var i = 0; i < bucket.length; i++) {
      var cand = bucket[i];
      if (cand.count < MIN_COUNT) continue;
      if (Math.abs(cand.len - phrase.length) > 2) continue;
      if (cand.t === phrase) return null;
      if (words.length === 1) {
        if (!isTypoOf(phrase, cand.t)) continue;
        var _score = boundedLevenshtein(phrase, cand.t, 2);
        if (_score < bestScore) {
          bestScore = _score;
          best = cand.t;
        }
        continue;
      }
      var cw = cand.t.split(/\s+/);
      if (cw.length !== words.length) continue;
      var differing = -1;
      var ok = true;
      for (var w = 0; w < words.length; w++) {
        if (words[w] === cw[w]) continue;
        if (differing !== -1) {
          ok = false;
          break;
        }
        differing = w;
      }
      if (!ok || differing === -1) continue;
      if (!isTypoOf(words[differing], cw[differing])) continue;
      var score = boundedLevenshtein(words[differing], cw[differing], 2);
      if (score < bestScore) {
        bestScore = score;
        best = cand.t;
      }
    }
    return best ? {
      tag: best,
      distance: bestScore
    } : null;
  }
  function splitSegments(text) {
    var segs = [];
    var start = 0;
    for (var i = 0; i <= text.length; i++) {
      if (i === text.length || text[i] === ',') {
        var raw = text.slice(start, i);
        var lead = raw.length - raw.replace(/^\s+/, '').length;
        var core = raw.trim();
        segs.push({
          coreStart: start + lead,
          core: core
        });
        start = i + 1;
      }
    }
    return segs;
  }
  function analyzePrompt(text, index, customSet, cache) {
    var segments = [];
    var known = 0,
      typo = 0,
      unknown = 0,
      total = 0;
    var _iterator5 = _createForOfIteratorHelper(splitSegments(text)),
      _step5;
    try {
      for (_iterator5.s(); !(_step5 = _iterator5.n()).done;) {
        var seg = _step5.value;
        var core = seg.core;
        var coreEnd = seg.coreStart + core.length;
        if (!core) {
          segments.push(_objectSpread(_objectSpread({}, seg), {}, {
            coreEnd: coreEnd,
            status: 'skip'
          }));
          continue;
        }
        if (/^<.*>$/.test(core)) {
          segments.push(_objectSpread(_objectSpread({}, seg), {}, {
            coreEnd: coreEnd,
            norm: core,
            status: 'skip'
          }));
          continue;
        }
        var norm = normalizeSegment(core);
        if (!norm) {
          segments.push(_objectSpread(_objectSpread({}, seg), {}, {
            coreEnd: coreEnd,
            status: 'skip'
          }));
          continue;
        }
        var res = void 0;
        if (cache && cache.has(norm)) {
          res = cache.get(norm);
        } else if (customSet && customSet.has(norm)) {
          res = {
            status: 'known',
            category: 'custom'
          };
        } else if (index.known.has(norm)) {
          res = {
            status: 'known',
            category: index.known.get(norm)
          };
        } else if (COMMON_WORDS.has(norm) || isInflectionOfKnown(norm, index.known)) {
          res = {
            status: 'ok'
          };
        } else {
          var s = suggestTag(norm, index);
          res = s ? {
            status: 'typo',
            suggestion: s.tag
          } : {
            status: 'unknown'
          };
        }
        if (cache && !(customSet && customSet.has(norm))) cache.set(norm, res);
        if (res.status === 'known') {
          known++;
          total++;
        } else if (res.status === 'typo') {
          typo++;
          total++;
        } else if (res.status === 'unknown') {
          unknown++;
          total++;
        }
        segments.push(_objectSpread(_objectSpread({}, seg), {}, {
          coreEnd: coreEnd,
          norm: norm
        }, res));
      }
    } catch (err) {
      _iterator5.e(err);
    } finally {
      _iterator5.f();
    }
    var healthPct = known + typo > 0 ? Math.round(known / (known + typo) * 100) : 100;
    return {
      segments: segments,
      stats: {
        known: known,
        typo: typo,
        unknown: unknown,
        total: total,
        healthPct: healthPct
      }
    };
  }
  var CUSTOM_KEY = 'promptSpellcheck_customWords';
  function loadCustomWords() {
    try {
      var raw = localStorage.getItem(CUSTOM_KEY);
      return new Set(raw ? JSON.parse(raw) : []);
    } catch (_unused2) {
      return new Set();
    }
  }
  function saveCustomWord(word) {
    try {
      var _set = loadCustomWords();
      _set.add(word);
      localStorage.setItem(CUSTOM_KEY, JSON.stringify(Array.from(_set)));
      return _set;
    } catch (_unused3) {
      return loadCustomWords();
    }
  }
  return {
    WEIGHT_MIN: WEIGHT_MIN,
    WEIGHT_MAX: WEIGHT_MAX,
    LORA_MIN: LORA_MIN,
    LORA_MAX: LORA_MAX,
    WEIGHT_STEP: WEIGHT_STEP,
    scanBrackets: scanBrackets,
    segmentPrompt: segmentPrompt,
    segmentAt: segmentAt,
    formatWeight: formatWeight,
    classifySegment: classifySegment,
    getSegmentWeight: getSegmentWeight,
    setSegmentWeight: setSegmentWeight,
    adjustSegmentWeight: adjustSegmentWeight,
    normalizeKey: normalizeKey,
    lintPrompt: lintPrompt,
    replaceRange: replaceRange,
    replaceSegment: replaceSegment,
    deleteSegment: deleteSegment,
    cleanInsert: cleanInsert,
    insertAfterSegment: insertAfterSegment,
    duplicateSegment: duplicateSegment,
    appendSegment: appendSegment,
    moveSegment: moveSegment,
    toggleUnderscores: toggleUnderscores,
    escapeTagForPrompt: escapeTagForPrompt,
    replaceBase: replaceBase,
    autocompleteContext: autocompleteContext,
    PROMPT_CATEGORIES: PROMPT_CATEGORIES,
    ORDER_FAMILIES: ORDER_FAMILIES,
    promptOrderFamily: promptOrderFamily,
    classifyPromptSegment: classifyPromptSegment,
    classifyAll: classifyAll,
    organizePrompt: organizePrompt,
    removeDuplicates: removeDuplicates,
    isPermutationOfSegments: isPermutationOfSegments,
    insertTagLogically: insertTagLogically,
    logicalMoveTarget: logicalMoveTarget,
    moveSegmentLogically: moveSegmentLogically,
    categoryStepTarget: categoryStepTarget,
    orderPreviewCategories: orderPreviewCategories,
    CATEGORY_COLORS: CATEGORY_COLORS,
    categoryColor: categoryColor,
    SUGGEST_MIN_COUNT: SUGGEST_MIN_COUNT,
    buildIndex: buildIndex,
    isInflectionOfKnown: isInflectionOfKnown,
    normalizeSegment: normalizeSegment,
    COMMON_WORDS: COMMON_WORDS,
    suggestTag: suggestTag,
    splitSegments: splitSegments,
    analyzePrompt: analyzePrompt,
    loadCustomWords: loadCustomWords,
    saveCustomWord: saveCustomWord
  };
}();