default:
	@just --list

alias c := commit
commit MSG:
	git add . && git commit -m "{{MSG}}" && git push 

asan_flags := "-fsanitize=address,leak,undefined -ftrapv"
warn_flags := "-Wall -Wextra -Wshadow -Wconversion"
include_flags := "-Iinclude"
std_flags := "-std=c99"

outfile := "owlvm"

source_files := "./src"

build:
	gcc \
		{{include_flags}} \
		{{std_flags}} \
		{{warn_flags}} \
		$(find {{source_files}} -type f -name "*.c") \
		-o {{outfile}}

