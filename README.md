# Speculative Out-of-Order CPU Simulator

A cycle-oriented functional and timing simulator for a configurable, speculative,
multi-issue, out-of-order processor. The simulator executes a small RISC-V-like
instruction set and reports pipeline timing and stall statistics. It can also
dump the final memory/register state and serialize results to JSON.

## Features

- Configurable fetch, dispatch, writeback, and commit widths
- Configurable instruction queue and reorder buffer sizes
- Register renaming with a physical register file and free list
- Reservation stations for integer, load/store, floating-point, and branch units
- A 16-entry branch target buffer with 2-bit saturating counters
- In-order retirement through a circular reorder buffer
- Common Data Bus arbitration, with write/capacity counters in JSON output
- Optional detailed final-state output and JSON serialization

The supported instructions are `fld`, `fsd`, `add`, `addi`, `slt`, `fadd`,
`fsub`, `fmul`, `fdiv`, and `bne`.

## Simulated Architecture

The functional-unit layout and reservation-station counts are fixed in the
current implementation:

| Unit | Latency | Reservation stations | Instructions |
| --- | ---: | ---: | --- |
| Integer | 1 cycle | 4 | `add`, `addi`, `slt` |
| Load/store | 1 cycle for address calculation | 2 load + 2 store | `fld`, `fsd` |
| Floating-point add | 3 cycles, pipelined | 3 | `fadd`, `fsub` |
| Floating-point multiply | 4 cycles, non-pipelined | 2 | `fmul` |
| Floating-point divide | 6 cycles, non-pipelined | 1 | `fdiv` |
| Branch | 1 cycle | 2 | `bne` |

The simulator uses 32 allocatable physical registers plus a dedicated constant
zero register. Stores update memory at commit so speculative execution remains
recoverable. On a branch misprediction, younger work is squashed and the rename
state is restored from a checkpoint.

## Requirements

- A POSIX-like environment, such as Linux or the included dev container
- A C++17 compiler
- GNU Make
- `pkg-config`
- GLib 2 development headers and libraries

On Ubuntu or Debian, the required build dependencies can be installed with:

```sh
sudo apt-get install build-essential pkg-config libglib2.0-dev
```

## Build and Run

Build the command-line simulator:

```sh
make
```

Run one of the included programs with a processor configuration:

```sh
./sim -p programs/prog-noloop.dat -c configs/4-wide.cfg
```

Write the final state to JSON:

```sh
./sim -p programs/prog-noloop.dat -c configs/4-wide.cfg \
  -j outputs/prog-noloop.4-wide.json
```

Both `-p` and `-c` are required. Add `-d` to print the final nonzero memory
entries, architectural-to-physical register mapping, and physical register
values. Run `./sim -h` to see the command-line options.

To execute every included program against every configuration and compare the
results with the checked-in reference outputs, run:

```sh
make run
```

Generated simulator output is written to `outputs/`; whitespace-insensitive
comparisons are written to `diffs/`.

## Program Format

Every program file must contain a `%%` separator. Integer-addressed memory
initialization entries may appear before it; the instruction stream follows it.
Files with no initial memory data still begin with `%%`, as shown by
`programs/simple-branch.dat` and several other included programs.

The following is an excerpt in the same format as `programs/prog-loop.dat`:

```text
% All registers initially read as 0.
% Memory content uses address, value pairs.
0, 111
8, 14
200, 12
%%
      addi X1, X0, 24
      fld F2, 200(X0)
loop: fld F0, 0(X1)
      fmul F0, F0, F2
      addi X1, X1, -8
      bne  X1, X0, loop
```

The parser behavior is:

- Leading and trailing whitespace is ignored.
- A line whose first non-whitespace character is `%` is a comment.
- Instruction lines may contain trailing `% ...` comments, as demonstrated by
  `programs/test33.dat`; tokens after the required operands are ignored.
- Memory entries use `integer_address, numeric_value` syntax. The simulator has
  1,024 memory slots, addressed from 0 through 1,023.
- Labels use `name:` syntax and may share a line with an instruction.
- Opcodes are case-insensitive. The included programs use `Xn` for integer
  registers and `Fn` for floating-point registers.
- `X0` always reads as zero. Other registers also initially read as zero until
  written.
- Each parsed instruction occupies four bytes of simulated instruction address
  space, starting at PC 0.

The parser accepts the following instruction forms:

```text
fld  Fd, offset(Xbase)
fsd  Fs, offset(Xbase)
add  Xd, Xs1, Xs2
addi Xd, Xs, immediate
slt  Xd, Xs1, Xs2
fadd Fd, Fs1, Fs2
fsub Fd, Fs1, Fs2
fmul Fd, Fs1, Fs2
fdiv Fd, Fs1, Fs2
bne  Xs1, Xs2, label
```

These register classes match the included test files. The parser identifies a
register from its `X` or `F` prefix but does not independently enforce opcode
operand types or register-number bounds.

## Configuration

Configuration files use GLib key-file syntax. All fields belong to the
`[pipeline]` section:

| Key | Meaning |
| --- | --- |
| `NF` | Maximum instructions fetched and decoded per cycle |
| `NI` | Capacity limit applied to both the fetch and decode queues |
| `NW` | Maximum instructions dispatched per cycle |
| `NR` | Reorder buffer capacity |
| `NB` | Common Data Bus/writeback width |
| `NC` | Maximum instructions committed per cycle |

Example:

```ini
[pipeline]
NF=4
NI=16
NW=4
NR=16
NB=4
NC=4
```

All six values must be positive. The example above matches
`configs/4-wide.cfg`; the executable does not select a default configuration,
so `-c` is always required. Seven ready-to-run configurations are available in
`configs/`.

## Output

Every run prints the total clock cycles, reservation-station stalls, and
reorder-buffer stalls. With `-j`, the JSON document contains:

- `total_cycles`, `rs_stalls`, and `rob_stalls`
- `cdb_writes` and `cdb_cycles_capacity`
- all 1,024 entries of `data_memory`
- all 32 allocatable entries of the physical `register_file`

The JSON register array is the physical register file, not an architectural
register map. Use `-d` when the final architectural-to-physical mapping is also
needed.

## XLSX Status

The repository contains an XLSX logger, an OpenXLSX dependency, a `make xlsx`
target, and pre-generated reference timelines. The logger is not currently
wired into the simulator: `setXlsxLogger` has no implementation and the
pipeline stages do not emit XLSX events. Consequently, the XLSX target is not
documented as a supported build or runtime feature in the current revision.

## Repository Layout

| Path | Contents |
| --- | --- |
| `src/` | Simulator and command-line implementation |
| `include/` | Simulator headers and bundled JSON header |
| `configs/` | Processor configurations |
| `programs/` | Example instruction streams |
| `outputs_solution/` | Reference text outputs used by `make run` |
| `outputs_verbose/` | Pre-generated reference cycle-by-cycle traces |
| `outputs_xlsx_solution/` | Pre-generated reference XLSX timelines |
| `OpenXLSX/` | Bundled dependency for the unfinished XLSX exporter |

Use `make clean` to remove simulator objects/binaries and files generated under
`outputs/`, `outputs_xlsx/`, and `diffs/`. It does not clean `OpenXLSX/build/`.
