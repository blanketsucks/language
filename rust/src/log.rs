use crate::tokens::Span;

use colored::*;

fn format_span(span: &Span) -> String {
    format!("{}:{}:{}", span.filename, span.start.line, span.start.column)
}

// TODO: Disable colors when not in a TTY
pub fn log_error(span: &Span, message: String) {
    println!("{} {} {}", format_span(span).bold(), "error:".red().bold(), message);

    let fmt = format!("{} |", span.start.line);
    println!("{} {}", fmt.bold(), span.line);
    
    let len = span.line.len();
    let offset = std::cmp::min(
        span.start.column - 1 + fmt.len(), len
    );

    let padding = if span.length() == 0 { 1 } else { span.length() };
    for _ in 0..offset {
        print!(" ");
    }

    for _ in 0..padding {
        print!("{}", "^".bold());
    }

    println!();
}

pub fn log_note(span: &Span, message: String) {
    println!("{} {} {}", format_span(span).bold(), "note:".purple().bold(), message);

    let fmt = format!("{} |", span.start.line);
    println!("{} {}", fmt.bold(), span.line);
    
    let len = span.line.len();
    let offset = std::cmp::min(
        span.start.column - 1 + fmt.len(), len
    );

    let padding = if span.length() == 0 { 1 } else { span.length() };
    for _ in 0..offset {
        print!(" ");
    }

    for _ in 0..padding {
        print!("{}", "^".bold());
    }

    println!();
}

pub fn default_log_error(message: String) {
    println!("{} {} {}", "quart:".bold(), "error:".red().bold(), message);
}

macro_rules! error {
    ($message:literal) => {
        { 
            use crate::log::default_log_error;

            default_log_error($message.to_string());
            std::process::exit(1);
        }
    };

    ($message:literal, $($arg:tt)*) => {
        {
            use crate::log::default_log_error;

            default_log_error(format!($message, $($arg)*));
            std::process::exit(1);
        }
    };

    ($span:expr, $message:expr) => {
        {
            use crate::log::log_error;

            log_error($span, $message.to_string());
            std::process::exit(1);
        }
    };

    ($span:expr, $message:expr, $($arg:tt)*) => {
        {
            use crate::log::log_error;

            log_error($span, format!($message, $($arg)*));
            std::process::exit(1);
        }
    };
}

macro_rules! note {
    ($span:expr, $message:expr) => {
        {
            use crate::log::log_note;
            log_note($span, $message.to_string());
        }
    };

    ($span:expr, $message:expr, $($arg:tt)*) => {
        {
            use crate::log::log_note;
            log_note($span, format!($message, $($arg)*));
        }
    };
}

pub(crate) use error;
pub(crate) use note;