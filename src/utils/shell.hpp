#ifndef SHELL_HPP
#define SHELL_HPP

/*
 * \file shell.hpp
 *
 * Contains definitions for various shell manipulators
 */

/// namespace for all shell manipulators
namespace shell {

#define ESC(VALUE) "\033" "[" VALUE "m"

/// Resets every settings to their default value
const char *const NORMAL = ESC("0");

/// Sets the font to bold typeface
const char *const BOLD = ESC("1");

/// Remove the bold typeface from the current font
const char *const NO_BOLD = ESC("21");

/// Slightly reduces the luminosity of the current color
const char *const DIM = ESC("2");

/// Restore the luminosity of the current color
const char *const NO_DIM = ESC("22");

/// Adds an underline to the text
const char *const UNDERLINED = ESC("4");

/// Disables underlining
const char *const NO_UNDERLINED = ESC("24");

/// Sets the font to blink
const char *const BLINK = ESC("5");

/// Disables blinking
const char *const NO_BLINK = ESC("25");

/// Inverts foreground/background colors
const char *const REVERSE = ESC("7");

/// Restore foregound/background colors
const char *const NO_REVERSE = ESC("27");

/// Hides the text (e.g. for passwords)
const char *const HIDDEN = ESC("8");

/// Restores text display
const char *const NO_HIDDEN = ESC("28");

/// namespace for shell foreground color manipulation
namespace foregrounds {

const char *const DEFAULT = ESC("39"); ///< Default foregound color for the console
const char *const   BLACK = ESC("30"); ///< Black foregound color
const char *const     RED = ESC("31"); ///< Red foregound color
const char *const   GREEN = ESC("32"); ///< Green foregound color
const char *const  YELLOW = ESC("33"); ///< Yellow foregound color
const char *const    BLUE = ESC("34"); ///< Blue foregound color
const char *const MAGENTA = ESC("35"); ///< Magenta foregound color
const char *const    CYAN = ESC("36"); ///< Cyan foregound color
const char *const    GRAY = ESC("37"); ///< Gray foregound color

const char *const     DARK_GRAY = ESC("90"); ///< Dark gray foregound color
const char *const     LIGHT_RED = ESC("91"); ///< Light red foregound color
const char *const   LIGHT_GREEN = ESC("92"); ///< Light green foregound color
const char *const  LIGHT_YELLOW = ESC("93"); ///< Light yellow foregound color
const char *const    LIGHT_BLUE = ESC("94"); ///< Light blue foregound color
const char *const LIGHT_MAGENTA = ESC("95"); ///< Light magenta foregound color
const char *const    LIGHT_CYAN = ESC("96"); ///< Light cyan foregound color
const char *const         WHITE = ESC("97"); ///< White foregound color

} // end of namespace foregrounds

/// namespace for shell background color manipulation
namespace backgrounds {

const char *const DEFAULT = ESC("49"); ///< Default backgound color for the console
const char *const   BLACK = ESC("40"); ///< Black backgound color
const char *const     RED = ESC("41"); ///< Red backgound color
const char *const   GREEN = ESC("42"); ///< Green backgound color
const char *const  YELLOW = ESC("43"); ///< Yellow backgound color
const char *const    BLUE = ESC("44"); ///< Blue backgound color
const char *const MAGENTA = ESC("45"); ///< Magenta backgound color
const char *const    CYAN = ESC("46"); ///< Cyan backgound color
const char *const    GRAY = ESC("47"); ///< Gray backgound color

const char *const     DARK_GRAY = ESC("100");  ///< Dark gray backgound color
const char *const     LIGHT_RED = ESC("101");  ///< Light red backgound color
const char *const   LIGHT_GREEN = ESC("102");  ///< Light green backgound color
const char *const  LIGHT_YELLOW = ESC("103");  ///< Light yellow backgound color
const char *const    LIGHT_BLUE = ESC("104");  ///< Light blue backgound color
const char *const LIGHT_MAGENTA = ESC("105");  ///< Light magenta backgound color
const char *const    LIGHT_CYAN = ESC("106");  ///< Light cyan backgound color
const char *const         WHITE = ESC("107");  ///< White backgound color

} // end of namespace backgrounds

} // end of namespace shell

#undef ESC
#endif // SHELL_HPP
