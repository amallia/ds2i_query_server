#ifndef INDEX_PARTITIONING_QUERY_STATIC_PARSER_HPP
#define INDEX_PARTITIONING_QUERY_STATIC_PARSER_HPP

#include <sstream>
#include <string>

#include "query_expr_and.hpp"
#include "query_expr_or.hpp"
#include "query_expr_term.hpp"
#include "query_parser_exception.hpp"
#include "query_scanner.hpp"
#include "query_scanner_token.hpp"


namespace query {
    class QueryStaticParser {
    public:
        /**
         * Parses the given string and tranform it into an equivalent %QEORExpr_c of %QECNFExpr_c
         * @param str The string to parse
         * @return A %QEORExpr_c equivalent to the given string
         * @throws QueryParserException If the string cannot be generated by the grammar used by this parser
         */
        template<typename T>
        static
        T
        parse(const std::string &str) {
            return QueryStaticParser::parse<T>(str.c_str());
        }

        /**
         * Parses the given string and tranform it into an equivalent %QEORExpr_c of %QECNFExpr_c
         * @param str The string to parse
         * @return A %QECNFExpr_c equivalent to the given string
         * @throws QueryParserException If the string cannot be generated by the grammar used by this parser
         */
        template<typename T>
        static
        T
        parse(const char *str) {
            QueryScanner scanner = QueryScanner(str);
            unsigned short number_tokens = 0;

            try {
                scanner.getNextToken();
                T result;
                QueryStaticParser::parse_impl(scanner, &number_tokens, result);
                QueryStaticParser::checkCurrentToken(scanner, {TOK_END});
                return std::move(result);
            } catch (const QueryParserException &e) {
                throw e;
            } catch (const std::exception &e) {
                std::stringstream ss;
                ss << QueryStaticParser::getDefaultError(scanner).what();
                ss << ". ";
                ss << e.what();
                throw QueryParserException(ss.str());
            }
        }

    private:
        /*
        template<typename T>
        static
        void
        parse_impl(QueryScanner & scanner, unsigned short *number_tokens, T & result);
         */

        /**
         * Parses the production: QueryExprAND ::= TOK_L_BRACKET T ( T )* TOK_R_BRACKET
         * @return The %QueryExprAND expression associated to this production
         * @throws QueryParserException If the production cannot be applied
         */
        template<typename T>
        static
        void
        parse_impl(QueryScanner &scanner, unsigned short *number_tokens, QueryExprAND<T> & result) {
            /// QueryExprAND ::= TOK_L_BRACKET T ( T )* TOK_R_BRACKET

            T sub_result;
            result.clear();

            QueryStaticParser::checkCurrentToken(scanner, {TOK_L_BRACKET});
            scanner.getNextToken();
            QueryStaticParser::parse_impl(scanner, number_tokens, sub_result);
            result &= sub_result;

            // I try to extend this production on the right with other AndExpr(s)
            while (scanner.getCurrentToken() != TOK_R_BRACKET) {
                QueryStaticParser::parse_impl(scanner, number_tokens, sub_result);
                result &= sub_result;
            }

            QueryStaticParser::checkCurrentToken(scanner, {TOK_R_BRACKET});
            scanner.getNextToken();
        }

        /**
         * Parses the production: QueryExprOR ::= TOK_L_BRACKET T ( TOK_OR T )* TOK_R_BRACKET
         * @return The %QueryExprOR expression associated to this production
         * @throws QueryParserException If the production cannot be applied
         */
        template<typename T>
        static
        void
        parse_impl(QueryScanner &scanner, unsigned short *number_tokens, QueryExprOR<T> & result) {
            /// QueryExprOR ::= TOK_L_BRACKET T ( TOK_OR T )* TOK_R_BRACKET

            T sub_result;
            result.clear();

            QueryStaticParser::checkCurrentToken(scanner, {TOK_L_BRACKET});
            scanner.getNextToken();

            QueryStaticParser::parse_impl(scanner, number_tokens, sub_result);
            result |= sub_result;

            // I try to extend this production on the right with other AndExpr(s)
            while (scanner.getCurrentToken() != TOK_R_BRACKET) {
                QueryStaticParser::checkCurrentToken(scanner, {TOK_OR});
                scanner.getNextToken();

                QueryStaticParser::parse_impl(scanner, number_tokens, sub_result);
                result |= sub_result;
            }

            QueryStaticParser::checkCurrentToken(scanner, {TOK_R_BRACKET});
            scanner.getNextToken();
        }

        /**
         * Parses the production: QueryExprTerm ::= TOK_TERM | TOK_DOUBLE_QUOTE TOK_TERM ( TOK_TERM )* TOK_DOUBLE_QUOTE
         * @return The %QueryExprTerm expression associated to this production
         * @throws QueryParserException If the production cannot be applied
         */
        static
        void
        parse_impl(QueryScanner &scanner, unsigned short *number_tokens, QueryExprTerm & result) {
            /// QueryExprTerm ::= TOK_TERM | TOK_DOUBLE_QUOTE ( TOK_TERM )* TOK_DOUBLE_QUOTE

            switch (scanner.getCurrentToken()) {
                case TOK_DOUBLE_QUOTE:
                    scanner.getNextToken();
                    {
                        std::stringstream ss;
                        bool add_space = false;
                        while (scanner.getCurrentToken() != TOK_DOUBLE_QUOTE) {
                            QueryStaticParser::checkCurrentToken(scanner, {TOK_TERM});
                            if (add_space)
                                ss << " ";
                            ss << std::string(scanner.uLexVal.str_ptr, scanner.uLexVal.str_size);
                            scanner.getNextToken();
                            add_space = true;
                        }
                        QueryStaticParser::checkCurrentToken(scanner, {TOK_DOUBLE_QUOTE});
                        scanner.getNextToken();

                        result = QueryExprTerm(ss.str(), (*number_tokens)++);
                    }
                break;

                case TOK_TERM:
                    result = QueryExprTerm(std::string(scanner.uLexVal.str_ptr, scanner.uLexVal.str_size), (*number_tokens)++);
                    scanner.getNextToken();
                break;

                default:
                    throw QueryStaticParser::getCustomError(scanner, {TOK_DOUBLE_QUOTE, TOK_TERM});
            }
        }

    private:
        /**
         * Check if the current token is among the expected ones and if it isn't throws an exception.
         * @throws QueryParserException If the current token is not among the expected ones
         */
        static
        void
        checkCurrentToken(QueryScanner &scanner, std::initializer_list<QueryScannerToken> expected_tokens) {
            QueryScannerToken current_token = scanner.getCurrentToken();
            for (const QueryScannerToken *expected_token = expected_tokens.begin();
                 expected_token != expected_tokens.end(); ++expected_token) {
                if (current_token == *expected_token)
                    return;
            }
            throw QueryStaticParser::getCustomError(scanner, expected_tokens);
        }

        /**
         * Gets an error message with informations about the context
         */
        static
        QueryParserException
        getDefaultError(QueryScanner &scanner) {
            return std::move(getCustomError(scanner, {}));
        }

        /**
         * Gets an error message with informations about the context and including what token we expected
         */
        static
        QueryParserException
        getCustomError(QueryScanner &scanner, std::initializer_list<QueryScannerToken> expected_tokens) {
            std::stringstream ss;
            ss << "Unexpected " << QueryScannerTokenToString(scanner.getCurrentToken()) << " token in position "
               << scanner.getCurrentPosition();

            switch (expected_tokens.size()) {
                case 0:
                    break;

                case 1:
                    ss << ", expected " << QueryScannerTokenToString(*expected_tokens.begin()) << " instead";
                    break;

                default:
                    ss << ", expected one of the following ones instead: ";
                    {
                        const QueryScannerToken *begin = expected_tokens.begin();
                        const QueryScannerToken *end = expected_tokens.end();
                        for (const QueryScannerToken *expected_token = begin; expected_token != end; ++expected_token) {
                            if (expected_token != begin)
                                ss << ", ";
                            ss << QueryScannerTokenToString(*expected_token);
                        }
                    }
                    break;
            }

            return QueryParserException(ss.str());
        }
    };
}

#endif //INDEX_PARTITIONING_QUERY_STATIC_PARSER_HPP
