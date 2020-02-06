all: build test
	@true

.DEFAULT: all

clean:
	bazel clean
	rm -rf man
	rm -rf docs/*.md

build:
	bazel build //...

test:
	bazel test //...

.PHONY: clean build test

bazel-bin/verilog/tools/lint/verilog_lint:
	bazel build //verilog/tools/lint:verilog_lint

bazel-bin/verilog/tools/syntax/verilog_syntax:
	bazel build //verilog/tools/syntax:verilog_syntax

bazel-bin/verilog/tools/formatter/verilog_format:
	bazel build //verilog/tools/formatter:verilog_format

tools: bazel-bin/verilog/tools/lint/verilog_lint bazel-bin/verilog/tools/syntax/verilog_syntax bazel-bin/verilog/tools/formatter/verilog_format
	@true

# Generate man pages from the tools
man/verilog_lint.1: bazel-bin/verilog/tools/lint/verilog_lint
	mkdir -p man
	gflags2man --help_flag="--helpfull" --dest_dir man bazel-bin/verilog/tools/lint/verilog_lint

man/verilog_syntax.1: bazel-bin/verilog/tools/syntax/verilog_syntax
	mkdir -p man
	gflags2man --help_flag="--helpfull" --dest_dir man bazel-bin/verilog/tools/syntax/verilog_syntax

man/verilog_format.1: bazel-bin/verilog/tools/formatter/verilog_format
	mkdir -p man
	gflags2man  --help_flag="--helpfull" --dest_dir man bazel-bin/verilog/tools/formatter/verilog_format

man/%.gz: man/%
	gzip -k -f $<

man: man/verilog_lint.1.gz man/verilog_syntax.1.gz man/verilog_format.1.gz
	@true

.PHONY: man

docs: docs/generate.sh *.md
	./docs/generate.sh

.PHONY: docs

PREFIX ?= /usr/local
PREFIX_BIN=$(PREFIX)/bin
PREFIX_BIN=$(PREFIX)/bin
PREFIX_DOC=$(PREFIX)/share/verible
PREFIX_MAN=$(PREFIX)/share/man/man1
install: man docs
	# Tools
	install -D -m755 bazel-bin/verilog/tools/syntax/verilog_syntax $(PREFIX_BIN)/verilog_syntax
	install -D -m755 bazel-bin/verilog/tools/lint/verilog_lint $(PREFIX_BIN)/verilog_lint
	install -D -m755 bazel-bin/verilog/tools/formatter/verilog_format $(PREFIX_BIN)/verilog_format
	# docs
	for PAGE in docs/*; do \
		install -D -m644 $$PAGE $(PREFIX_DOC)/$$(basename $$PAGE); \
	done
	# man pages
	install -D -m644 man/verilog_lint.1.gz   $(PREFIX_MAN)/verilog_lint.1.gz
	install -D -m644 man/verilog_syntax.1.gz $(PREFIX_MAN)/verilog_syntax.1.gz
	install -D -m644 man/verilog_format.1.gz $(PREFIX_MAN)/verilog_format.1.gz

.PHONY: install
