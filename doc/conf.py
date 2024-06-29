# -*- coding: utf-8 -*-

from datetime import datetime

extensions = ['myst_parser']
templates_path = ['templates', '_templates', '.templates']
source_suffix = ['.rst', '.md']
source_parsers = {
      '.rst': 'restructuredtext',
      '.md': 'markdown',
}
master_doc = 'index'
project = u'verible'
copyright = str(datetime.now().year)
version = 'latest'
release = 'latest'
exclude_patterns = ['_build']
pygments_style = 'sphinx'
htmlhelp_basename = 'verible'
html_theme = 'sphinx_rtd_theme'
file_insertion_enabled = False
latex_documents = [
  ('index', 'verible.tex', u'verible Documentation', u'', 'manual'),
]
