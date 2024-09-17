#ifndef FILE_PICKER_H
#define FILE_PICKER_H

#include <stddef.h>
#include <stdbool.h>
#include <ff.h>

#include "main_menu.h"

typedef bool (*file_picker_filter_func_t)(const FILINFO *fno);

/**
 * Show a file picker, returning the selected file as an altname path
 *
 * @param title Title to show on the picker screens
 * @param filepath Resulting file path, in altname format
 * @param len Length of the filepath buffer
 *
 * @retval MENU_OK If a filename was selected
 * @retval MENU_CANCEL If no file was selected
 * @retval MENU_TIMEOUT If the picker had a timeout
 */
menu_result_t file_picker_show(const char *title, char *filepath, size_t len, file_picker_filter_func_t filter_func);

/**
 * Find the full long filename corresponding to an altname path.
 *
 * @param filename Output buffer for the long filename
 * @param len Length of the output buffer
 * @param filepath Complete file path, in altname format
 * @return True if expanded, false otherwise
 */
bool file_picker_expand_filename(char *filename, size_t len, const char *filepath);

#endif /* FILE_PICKER_H */
