"""
This script generates the documentation for the options of the SAT solver.
It reads the file src/solver/SAT-options.hpp and generates a text manual.
Simply run the script and copy the output to the manual after updating the
description of the options
"""

import pandas as pd

header = '''
NapSAT is a CDCL SAT solver  supporting both  chronological and  non-chronological backtracking. The
solver will print "s SATISFIABLE" if the problem has a solution, and "s UNSATISFIABLE" otherwise.

By default, Boolean options [on/off] are set [on] if the specification is omitted

To use the solver, use the following command:
    NapSAT <input_file/-h/-hs/-hn> [options]

The options are the following:
********************************************* HELPERS **********************************************
  -h or --help
    Print this helper

  -hs or --help-sat-commands
    Print the documentation of SAT commands the interactive solver supports

  -hn or --help-navigation
    Print the documentation of the navigation commands the observer supports
'''
option_file = "include/SAT-options.hpp"

columns = [
    "Category",
    "Option",
    "Alias",
    "Type",
    "Default",
    "Description",
    "Requires",
    "Subsumed by",
    "Warning",
]

tagToColumn = {
    "@brief": "Description",
    "@alias": "Alias",
    "@requires": "Requires",
    "@subsumed": "Subsumed by",
    "@warning": "Warning",
    "@default": "Default",
}

dataframe = pd.DataFrame(columns=columns)


# Read the option file
with open(option_file, "r") as f:
    lines = f.readlines()
    currentCategory = ""
    currentTag = ""
    currentTagValue = ""

    document = False
    for line in lines:
        line = line.strip()
        # print(line)
        if line.find("/**") != -1 and line.find("**/") != -1:
            currentCategory = line.split("/**")[1].split("**/")[0].strip()
            if currentCategory == "Stop Documentation":
                document = False
            if currentCategory == "Start Documentation":
                document = True
            continue
        if not document:
            continue
        if line.find("/**") != -1:
            # add a line to the dataframe
            dataframe.loc[len(dataframe)] = [""] * len(columns)
            continue
        if line.find("*/") != -1:
            if currentTagValue != "":
                if currentTag not in tagToColumn:
                    print('Warning: unknown tag "' + currentTag + '"')
                elif tagToColumn[currentTag] not in dataframe.columns:
                    print(
                        'Warning: unknown column "' + tagToColumn[currentTag] + '"'
                    )
                else:
                    dataframe.loc[len(dataframe) - 1,
                                  tagToColumn[currentTag]
                    ] = currentTagValue
            currentTag = ""
            currentTagValue = ""
            # the next line should contain the name of the option
            continue

        if line.find("*") != -1:
            if line.find("@") != -1:
                if currentTagValue != "":
                    if currentTag not in tagToColumn:
                        print('Warning: unknown tag "' + currentTag + '"')
                    elif tagToColumn[currentTag] not in dataframe.columns:
                        print(
                            'Warning: unknown column "'
                            + tagToColumn[currentTag]
                            + '"'
                        )
                    else:
                        dataframe.loc[len(dataframe) - 1,
                                      tagToColumn[currentTag]
                        ] = currentTagValue
                currentTag = "@" + line.split("@")[1].split(" ")[0].strip()
                currentTagValue_list = line.split("@")[1].split(" ")[1:]
                currentTagValue = ""
                for word in currentTagValue_list:
                    currentTagValue += word + " "
                currentTagValue = currentTagValue.strip()
            else:
                currentTagValue += " " + line.split("*")[1].strip()
        if line.find("bool") != -1:
            dataframe.loc[len(dataframe) - 1, "Type"] = "bool"
            if line.find("=") != -1:
                dataframe.loc[len(dataframe) - 1, "Option"] = "--" + line.split("bool")[
                    1
                ].split("=")[0].strip().replace("_", "-")
                dataframe.loc[len(dataframe) - 1, "Default"] = (
                    line.split("=")[1]
                    .split(";")[0]
                    .strip()
                    .replace("true", "on")
                    .replace("false", "off")
                )
            else:
                dataframe.loc[len(dataframe) - 1, "Option"] = "--" + line.split("bool")[
                    1
                ].split(";")[0].strip().replace("_", "-")
            dataframe.loc[len(dataframe) - 1, "Category"] = currentCategory
            continue
        if line.find("double") != -1:
            dataframe.loc[len(dataframe) - 1, "Type"] = "double"
            dataframe.loc[len(dataframe) - 1, "Option"] = "--" + line.split("double")[
                1
            ].split("=")[0].strip().replace("_", "-")
            dataframe.loc[len(dataframe) - 1, "Default"] = (
                line.split("=")[1].split(";")[0].strip()
            )
            dataframe.loc[len(dataframe) - 1, "Category"] = currentCategory
            continue
        if line.find("std::string") != -1:
            dataframe.loc[len(dataframe) - 1, "Type"] = "string"
            dataframe.loc[len(dataframe) - 1, "Option"] = "--" + line.split(
                "std::string"
            )[1].split("=")[0].strip().replace("_", "-")
            if line.find("=") != -1:
                dataframe.loc[len(dataframe) - 1, "Default"] = (
                    line.split("=")[1].split(";")[0].strip()
                )
            dataframe.loc[len(dataframe) - 1, "Category"] = currentCategory
            continue


def justify_to_length(line, length):
    """
    Ensures that the line has exactly length characters.
    If a line is shorter than length, it is padded with between the longest words.
    """
    # remove double spaces
    while line.find("  ") != -1:
        line = line.replace("  ", " ")
    number_of_words = len(line.split(" "))
    if len(line) >= length:
        print("Warning: line is too long to be justified")
        return line
    if number_of_words < length - len(line):
        # the line is too short to be justified
        return line
    else:
        # find the longest words and add spaces between them
        # do not count the last word, as it is not followed by a space
        words = line.split(" ")[:-1]
        longest_words = set()
        words = sorted(words, key=lambda x: len(x), reverse=True)
        for i in range(length - len(line)):
            longest_words.add(words[i % len(words)])
        printed_line = ""
        for word in line.split(" "):
            if word in longest_words:
                printed_line += word + "  "
                longest_words.remove(word)
            else:
                printed_line += word + " "
        # remove the last space
        printed_line = printed_line[:-1]
        assert len(word) <= length
        return printed_line


def split_line_to_length(line, length, tab_length):
    if len(line) <= length - tab_length:
        return " " * tab_length + line
    else:
        # find the last space before the length
        last_space = line[: length - tab_length].rfind(" ")
        if last_space == -1:
            return (
                " " * tab_length
                + line[: length - tab_length]
                + "\n"
                + split_line_to_length(line[length - tab_length :], length, tab_length)
            )
        else:
            return (
                " " * tab_length
                + justify_to_length(line[:last_space], length - tab_length)
                + "\n"
                + split_line_to_length(line[last_space + 1 :], length, tab_length)
            )


# Write the dataframe to the standard output
with open("man.txt", "w") as f:
    f.write(header + "\n")
    last_category = ""
    for i in range(len(dataframe)):
        option_line = dataframe.loc[i]
        if option_line["Category"] != last_category:
            category_string = option_line["Category"]
            total_length = len(category_string)
            max_length = 100
            pading = int((max_length - total_length - 2) / 2)
            category_string = "*" * pading + " " + category_string + " " + "*" * pading
            if len(category_string) < max_length - 2:
                category_string += "*"
            last_category = option_line["Category"]
            f.write(category_string + "\n")
        option_string = ""

        # print the name of the option. If there is an alias, print it as well
        option_string += " " * 2
        if option_line["Alias"] != "":
            option_string += option_line["Alias"] + " or "
        option_string += option_line["Option"]
        if option_line["Type"] != "":
            option_string += (
                " <" + option_line["Type"] + " = " + option_line["Default"] + ">"
            )
        option_string += "\n"

        for column in columns:
            if (
                column == "Category"
                or column == "Option"
                or column == "Alias"
                or column == "Type"
                or column == "Default"
            ):
                continue
            if option_line[column] != "":
                if column != "Description":
                    option_line[column] = column + ": " + option_line[column]
                option_string += (
                    split_line_to_length(option_line[column], max_length, 4) + "\n"
                )
        f.write(option_string)
        if i != len(dataframe) - 1:
            f.write("\n")
        else:
            f.write("\n" + "*" * max_length + "\n")
