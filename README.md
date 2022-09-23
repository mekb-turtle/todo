# To-Do List:

Usage:

| command |   arguments   |                description                |
|---------|---------------|-------------------------------------------|
|add      |[text]         |adds an entry to the list                  |
|remove   |[index]        |removes an entry at index from the list    |
|todo     |[index]        |marks an entry at index as to-do           |
|done     |[index]        |marks an entry at index as done            |
|edit     |[index] [text] |edits an entry at index                    |
|list     |               |list all entries                           |
|clear    |               |clear completely                           |

Use another file using `$TODO_LOCATION` environment variable,\
Example:

    #!/bin/sh
    TODO_LOCATION="$(dirname -- "$(realpath -- "${BASH_SOURCE[0]:-.}")")/todo" todo "$@"
