# VOID ASCII Adventure

Find both treasures in the 12×12 dungeon and reach the exit. From the checkout
with the LLVM 23.1.4 VOID toolchain installed:

```sh
voidc check examples/ascii_adventure/main.void
voidc run examples/ascii_adventure/main.void --no-cache
```

In an interactive terminal, move with W/A/S/D or the arrow keys. Tap Q or Esc to
quit. Movement needs no Enter. Walls, the locked exit and invalid input leave
the turn count unchanged. On terminals that report key release, only key-down
events act. The game draws in place when stdout supports clearing and separates
frames otherwise.

For redirected input or line-oriented use, select `--line` explicitly. Use
W/A/S/D or Q followed by Enter. EOF exits cleanly. The following reaches a wall
and quits without advancing the turn count:

```sh
printf 'w\nq\n' | voidc run examples/ascii_adventure/main.void --no-cache -- --line
```

A native executable accepts the same option directly:

```sh
voidc build examples/ascii_adventure/main.void -o /tmp/void-adventure
/tmp/void-adventure
```

On Windows, choose a local output such as `adventure.exe` and run it in a
native console. Keyboard mode checks stdin capabilities, then holds a scoped
noncanonical/no-echo session across redraw and input. Normal and checked error
exits restore the prior input mode. Forced process termination is outside the
scope of that cleanup. Default keyboard mode reports an error on redirected
stdin and points to `--line`.

The five modules come from the supplied Adventure. `main.void` selects the
input path, `game.void` maps either input to the same movement logic,
`world.void` holds the map, `player.void` holds player coordinates and direction
helpers, and `renderer.void` draws the frame. The `ascii_adventure` test covers
both JIT modes and independent AOT, including walls, treasures, locked exit,
victory, invalid input, EOF and redirection.

[Qualification and source provenance](../../docs/developer-experience/G17_ASCII_ADVENTURE.md) · [Console input contract](../../docs/stage10/CONSOLE_IO.md#choosing-key-or-line-input)
