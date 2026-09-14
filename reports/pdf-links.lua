-- PDF files live one directory below the Markdown sources.
function Link(link)
  if link.target:match("^%a[%w+.-]*:") or link.target:sub(1, 1) == "#" then
    return link
  end
  local filename = link.target:match("([^/]+)$")
  if link.target:sub(-1) == "/" or (filename and not filename:find(".", 1, true)) then
    return pandoc.Span(link.content)
  end
  if link.target:match("^report_[^/]+%.md$") then
    link.target = link.target:gsub("%.md$", ".pdf")
  else
    link.target = "../" .. link.target
  end
  return link
end
