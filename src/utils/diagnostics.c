#include "vassian_utils.h"
#include <stdio.h>
#include <stdarg.h>

static const char* kind_to_str(DiagnosticKind kind) {
    switch (kind) {
        case DIAG_ERROR: return "\033[31merror\033[0m";
        case DIAG_WARNING: return "\033[33mwarning\033[0m";
        case DIAG_NOTE: return "\033[36mnote\033[0m";
    }
    return "unknown";
}

static void print_location(const SourceLoc* loc) {
    if (loc && loc->file) {
        fprintf(stderr, "%s:%zu:%zu: ", loc->file, loc->line, loc->column);
    }
}

void diag_report(DiagnosticKind kind, const Diagnostic* diag) {
    print_location(&diag->loc);
    fprintf(stderr, "%s", kind_to_str(kind));
    if (diag->error_code) {
        fprintf(stderr, "[%s]", diag->error_code);
    }
    fprintf(stderr, ": %s\n", diag->message);
    if (diag->note) {
        print_location(&diag->loc);
        fprintf(stderr, "\033[36mnote\033[0m: %s\n", diag->note);
    }
    if (diag->help) {
        print_location(&diag->loc);
        fprintf(stderr, "\033[32mhelp\033[0m: %s\n", diag->help);
    }
}

void diag_error(const SourceLoc* loc, const char* message, const char* error_code) {
    Diagnostic diag = { .loc = *loc, .message = message, .error_code = error_code };
    diag_report(DIAG_ERROR, &diag);
}

void diag_warning(const SourceLoc* loc, const char* message) {
    Diagnostic diag = { .loc = *loc, .message = message };
    diag_report(DIAG_WARNING, &diag);
}

void diag_note(const SourceLoc* loc, const char* message) {
    Diagnostic diag = { .loc = *loc, .message = message };
    diag_report(DIAG_NOTE, &diag);
}