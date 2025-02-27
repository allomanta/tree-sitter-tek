import XCTest
import SwiftTreeSitter
import TreeSitterTek

final class TreeSitterTekTests: XCTestCase {
    func testCanLoadGrammar() throws {
        let parser = Parser()
        let language = Language(language: tree_sitter_tek())
        XCTAssertNoThrow(try parser.setLanguage(language),
                         "Error loading Tek grammar")
    }
}
