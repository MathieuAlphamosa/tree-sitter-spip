# tree-sitter-spip

Tree-sitter grammar for the [SPIP](https://www.spip.net/) template language.

SPIP is a French CMS whose templates (`.html` files) mix HTML with SPIP-specific constructs: loops (boucles), tags (balises), filters, includes, multilingual blocks, and conditional brackets.

## Supported constructs

| Construct | Syntax | Example |
|-----------|--------|---------|
| Comment | `[(#REM) ... ]` | `[(#REM) Page header ]` |
| Loop | `<BOUCLE_name(TYPE){criteria}>` | `<BOUCLE_art(ARTICLES){par date}>` |
| Loop close | `</BOUCLE_name>` | `</BOUCLE_art>` |
| Loop conditional | `<B_name>`, `</B_name>`, `<//B_name>` | `<B_art>`, `</B_art>` |
| Include | `<INCLURE{params} />` | `<INCLURE{fond=header,env} />` |
| Tag (balise) | `(#NAME\|filter{param})` | `(#TITRE\|couper{80})` |
| Shorthand tag | `#NAME` | `#TITRE`, `#ENV{var}` |
| Filter | `\|name{param}` | `\|image_reduire{200}` |
| Multilingual | `<multi>[lang]text</multi>` | `<multi>[fr]Bonjour[en]Hello</multi>` |
| Translation | `<:module:string:>` | `<:spip:titre_page:>` |
| Conditional bracket | `[ ... ]` | `[<p>(#SOUSTITRE)</p>]` |

## Architecture

This grammar follows the **Twig-style approach**: it recognises only SPIP-specific constructs and treats everything else as `content` nodes. HTML is then injected into those content nodes via `injections.scm` in the editor extension (Zed, Neovim, etc.).

An **external scanner** (`src/scanner.c`) disambiguates characters that could start either a SPIP construct or HTML content (e.g., `<`, `#`, `(`).

## Usage

```bash
npm install
npx tree-sitter generate
npx tree-sitter test
```

## Used by

- [zed-spip](https://github.com/MathieuAlphamosa/zed-spip) - SPIP extension for the Zed editor

## Genesis

This tree-sitter grammar was vibecoded with [Claude Code](https://claude.ai/), Anthropic's AI coding agent, starting from an existing [Sublime Text extension](https://github.com/MathieuAlphamosa/Sublime-SPIP-AM) (TextMate regex-based grammar). The grammar, external scanner, and test corpus were designed and iterated through conversation, tested against 15 real-world SPIP skeleton files from production sites.

## License

MIT
