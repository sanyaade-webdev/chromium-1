// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Whitelist of tag names allowed in parseHtmlSubset.
 * @type {[string]}
 */
var allowedTags = ['A', 'B', 'STRONG'];

/**
 * Parse a very small subset of HTML.
 * @param {string} s The string to parse.
 * @throws {Error} In case of non supported markup.
 * @return {DocumentFragment} A document fragment containing the DOM tree.
 */
var allowedAttributes = {
  'href': function(node, value) {
    // Only allow a[href] starting with http:// and https://
    return node.tagName == 'A' && (value.indexOf('http://') == 0 ||
        value.indexOf('https://') == 0);
  },
  'target': function(node, value) {
    // Allow a[target] but reset the value to "".
    if (node.tagName != 'A')
      return false;
    node.setAttribute('target', '');
    return true;
  },
};

/**
 * Parse a very small subset of HTML.  This ensures that insecure HTML /
 * javascript cannot be injected into the new tab page.
 * @param {string} s The string to parse.
 * @param {array=} extraTags Extra allowed tags.
 * @param {object=} extraAttrs Extra allowed attributes (all tags are run
 *     through these).
 * @throws {Error} In case of non supported markup.
 * @return {DocumentFragment} A document fragment containing the DOM tree.
 */
function parseHtmlSubset(s, extraTags, extraAttrs) {
  function merge() {
    var clone = {};
    for (var i = 0; i < arguments.length; ++i) {
      if (typeof arguments[i] == 'object') {
        for (var key in arguments[i]) {
          if (arguments[i].hasOwnProperty(key))
            clone[key] = arguments[i][key];
        }
      }
    }
    return clone;
  }

  function walk(n, f) {
    f(n);
    for (var i = 0; i < n.childNodes.length; i++) {
      walk(n.childNodes[i], f);
    }
  }

  function assertElement(node) {
    if (tags.indexOf(node.tagName) == -1)
      throw Error(node.tagName + ' is not supported');
  }

  function assertAttribute(attrNode, node) {
    var n = attrNode.nodeName;
    var v = attrNode.nodeValue;
    if (!attrs.hasOwnProperty(n) || !attrs[n](node, v))
      throw Error(node.tagName + '[' + n + '="' + v + '"] is not supported');
  }

  var tags = allowedTags.concat(extraTags);
  var attrs = merge(allowedAttributes, extraAttrs);

  var r = document.createRange();
  r.selectNode(document.body);
  // This does not execute any scripts.
  var df = r.createContextualFragment(s);
  walk(df, function(node) {
    switch (node.nodeType) {
      case Node.ELEMENT_NODE:
        assertElement(node);
        var attrs = node.attributes;
        for (var i = 0; i < attrs.length; ++i) {
          assertAttribute(attrs[i], node);
        }
        break;

      case Node.COMMENT_NODE:
      case Node.DOCUMENT_FRAGMENT_NODE:
      case Node.TEXT_NODE:
        break;

      default:
        throw Error('Node type ' + node.nodeType + ' is not supported');
    }
  });
  return df;
}
