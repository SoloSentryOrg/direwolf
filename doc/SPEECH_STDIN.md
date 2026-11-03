# SPEECH_STDIN migration

Dire Wolf's former `SPEECH <script>` command passed received packet text as a
shell argument. That form is disabled. Configure the versioned interface with:

```text
SPEECH_STDIN /absolute/path/to/runner
```

On Windows, use a local drive path to an `.exe` or `.bat` runner, such as
`C:\DireWolf\dwespeak.bat`. The path must be shorter than 80 bytes and contain
only letters, digits, spaces, `._-`, a drive colon and path separators. The
application resolves and checks the runner at configuration time and again
before use. Unix runners must be executable regular files at absolute paths.
Quote a configured path that contains spaces. Symlink or reparse-point runner
files are rejected.

The runner receives the radio channel as its sole decimal argument. It reads
the packet text from **standard input** until end of file. Dire Wolf supplies
between 1 and 2048 bytes and rejects NUL and other non-text control bytes;
tabs and line breaks are allowed. The bytes are never shell or command-line
arguments. The bundled `scripts/dwespeak.sh` and `scripts/dwespeak.bat` now
forward standard input to eSpeak. Custom runners must make the same change and
must not interpolate standard input into a shell command. Use an absolute path
to a trusted local runner; configuration files remain operator-controlled.

SPEECH_STDIN is available on macOS, supported Unix systems and Windows Vista
or later. The core application still supports Windows XP, but SPEECH_STDIN
fails closed there because XP lacks the restricted-handle process API. The
legacy `SPEECH` directive does not execute or fall back to `system()` on any
platform. A missing, invalid or unsupported runner leaves speech disabled.

The child has a 30-second limit and receives only the bounded message on
standard input, a numeric channel argument and a small fixed environment on
Unix. Its output is discarded. Dire Wolf turns off the transmitter after a
child failure or timeout. A custom runner should exit nonzero on error and
should finish any synthesizer process before exiting.

To migrate a custom script, replace its use of the old second argument with a
read from standard input. For shell, use a program that accepts stdin directly
where possible, such as the bundled eSpeak example; avoid command substitution
that would reintroduce a shell boundary. Test the new runner with ordinary,
multiline and punctuation-heavy text before enabling transmission.

If the new runner fails after rollout, disable SPEECH_STDIN while preparing a
reviewed correction. Restoring the previous signed revision is safe only with
the vulnerable legacy SPEECH feature disabled. Do not restore packet-to-shell
execution as a fallback.
