package tree_sitter_tek_test

import (
	"testing"

	tree_sitter "github.com/tree-sitter/go-tree-sitter"
	tree_sitter_tek "github.com/allomanta/tree-sitter-tek/bindings/go"
)

func TestCanLoadGrammar(t *testing.T) {
	language := tree_sitter.NewLanguage(tree_sitter_tek.Language())
	if language == nil {
		t.Errorf("Error loading Tek grammar")
	}
}
