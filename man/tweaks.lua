-- This file contains some hooks that get invoked by Pandoc when converting
-- the manual files into plain text. They make the result look a tiny bit
-- nicer.

-- This converts URL targets into footnotes. So input Markdown text like:
--   [asdf](http://example.com/)
-- in the plain text manual looks like:
--   asdf[1]
--   [1] http://example.com/
function Link(el)
  return {
    el,
    pandoc.Note({ pandoc.Para({ pandoc.Str(el.target) }) })
  }
end

-- This attaches '=' characters at the start of headings, to distinguish them
-- as headings in plain text form.
function Header(el)
  local text = pandoc.utils.stringify(el.content)
  el.content = { pandoc.Str(string.rep("=", el.level) .. " " .. text) }
  return el
end
