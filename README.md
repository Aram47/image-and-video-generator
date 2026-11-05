# C++ Procedural People Image and Video Generator

A C++20 server that turns a text prompt into a stylized still image or a short MP4 of a person. Generation is procedural: a fixed vocabulary, mesh construction, a CPU rasterizer, and (when FFmpeg is available) libav encoding. The program does not call an AI API and does not load a pretrained image or video model.

Clients talk to the server over TCP with newline-delimited JSON (NDJSON). The JSON field `type` selects the operation: `image`, `video`, or `status`. Rendering runs on a worker thread; the socket thread only validates the request and returns a `job_id`.

## What the generator can draw

The parser recognizes only words and phrases that are implemented in its vocabulary. Unknown words are recorded on the completed job as `diagnostics.unsupported_terms` and do not change the scene. A small set of unsupported features (`wings`, `tail`, `horns`, `beard`, `mustache`, `hat`, `glasses`, `wheelchair`, `horse`, `dog`, `car`) fails the job with `UNSUPPORTED_PROMPT_FEATURE`.

Supported attributes:

| Attribute | Values |
| --- | --- |
| Gender presentation | `woman` (`female`, `lady`), `man` (`male`, `guy`) |
| Height | `short`, `average`, `tall` |
| Hair color | `black`, `brown`, `blonde`, `red` |
| Hair style | `straight`, `curly`, `short cut` |
| Clothing | `shirt`, `coat` (`overcoat`), `jacket` |
| Clothing color | `blue`, `green`, `yellow`, `red`, `black`, `white` |
| Place | `city street` (`street`), `park`, `plain` |
| Time of day | `day`, `sunset` (`dusk`), `night` |
| Pose | `standing` (`stands`), `sitting` (`sits`), `walking` (`walk`, `walks`), `waving` (`wave`, `waves`) |

The only render style is `stylized_3d`. The camera is a fixed front, medium shot. The same prompt and the same `seed` produce the same procedural variation. Attributes named in the prompt override that variation.

Video actions are `walk`, `wave`, and `walk_and_wave`. Pass the action in `options.action`. If it is omitted, the server infers it from the prompt (`walking` and `waving` together become `walk_and_wave`; otherwise a single verb wins; the default is `walk`).

Example prompt:

```text
A tall woman with curly red hair wearing a blue coat, standing on a city street at sunset
```

## Requirements

- CMake 3.20 or newer
- A C++20 compiler
- OpenSSL
- pkg-config and FFmpeg development libraries (`libavcodec`, `libavformat`, `libavutil`, `libswscale`) if you want MP4 output

Without libav the server still builds. Video jobs then fail at encode time because the null encoder is used. Still images do not need FFmpeg.

On macOS with Homebrew:

```bash
brew install cmake openssl pkg-config ffmpeg
```

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

This produces:

- `build/generator_server` — the TCP server
- `build/generator_cli` — a small client that submits a job and polls until it finishes
- `build/generator_tests` — the GoogleTest suite

Useful CMake options:

| Option | Default | Effect |
| --- | --- | --- |
| `GEN_ENABLE_FFMPEG` | `ON` | Look for libav and link the MP4 encoder when it is found |
| `GEN_ENABLE_ASAN` | `OFF` | Also link AddressSanitizer. The default sanitizer is UndefinedBehaviorSanitizer only |

nlohmann/json and GoogleTest are downloaded by CMake at configure time.

## Run the server

With no config file the server listens on `127.0.0.1:7070`, authentication is off, jobs are stored in `data/jobs`, and media is written to `outputs`. One worker processes the queue.

```bash
./build/generator_server
./build/generator_server /path/to/server_config.json
```

Stop the process with SIGINT or SIGTERM. A second process that uses the same data directory exits with `Server is already running.`

Example `server_config.json`:

```json
{
  "bind_address": "127.0.0.1",
  "port": 7070,
  "auth_required": false,
  "auth_token_file": "secrets/token",
  "tls_cert": "secrets/server.crt",
  "tls_key": "secrets/server.key",
  "output_dir": "outputs",
  "job_dir": "data/jobs",
  "max_queued": 8,
  "max_output_bytes": 2147483648,
  "log_prompts": false,
  "frame_retention_seconds": 0
}
```

Loading a config file turns authentication on unless `auth_required` is `false`. The token file holds a single line. Every request must then include the same value in `auth_token`, or the server responds with `UNAUTHORIZED`.

TLS is optional on `127.0.0.1`. Any other bind address requires both `tls_cert` and `tls_key`. The certificate and the key must be supplied together.

`frame_retention_seconds` is how long encoded frame directories may remain. `0` deletes abandoned frame directories when the server starts. Intermediate frames are removed after a successful encode. `log_prompts` controls whether prompt text is written to the log; metrics never include prompts or tokens.

Counters are snapshotted to `data/metrics.json` (`jobs_accepted`, `jobs_completed`, `jobs_failed`, `jobs_rejected`, `queue_depth`).

## Command-line client

`generator_cli` sends one request, prints each NDJSON response, and for `image` and `video` polls `status` until the job is `completed` or `failed`.

```bash
./build/generator_cli image \
  --prompt "A short man with black hair wearing a yellow shirt, standing" \
  --width 512 --height 512 --seed 42

./build/generator_cli video \
  --prompt "A young man in a green jacket walks through a park and waves" \
  --width 640 --height 360 --fps 12 --duration 2 \
  --action walk_and_wave --seed 42

./build/generator_cli status --job-id job-0123456789abcdef
```

| Flag | Default | Meaning |
| --- | --- | --- |
| `--host` | `127.0.0.1` | Server address |
| `--port` | `7070` | Server port |
| `--token` | empty | Sent as `auth_token` when authentication is enabled |
| `--ca-file` | empty | PEM file used to verify the server certificate. Required when the server speaks TLS |
| `--poll-interval` | `1` | Seconds between status polls |
| `--timeout` | `120` | Overall wait before the client prints `TIMEOUT` and exits 1 |
| `--prompt` | required for image and video | Prompt text |
| `--width`, `--height` | required for image and video | Frame size in pixels |
| `--fps`, `--duration`, `--action` | required for video | Frame rate, length in seconds, and action name |
| `--seed` | omitted | Unsigned 32-bit seed. The server chooses one when this is left out |
| `--job-id` | required for status | Job to query |

Exit code `0` means the job completed. Exit code `2` means the command line itself was invalid. Any other failure exits with code `1` and prints the server error code on stderr.

## Protocol

Transport is TCP. Each message is one UTF-8 JSON object followed by `\n`. The server reads until the newline, then writes one JSON object and a newline. The default maximum message size is 64 KiB.

Generation is asynchronous. `image` and `video` return `status: "queued"` and a `job_id`. A later `status` request reports the job.

Image request:

```json
{"protocol_version":1,"request_id":"r-1","type":"image","prompt":"A tall woman with curly red hair wearing a blue coat, standing on a city street at sunset","options":{"width":512,"height":512,"seed":12345,"style":"stylized_3d"}}
```

Video request:

```json
{"protocol_version":1,"request_id":"r-2","type":"video","prompt":"A young man in a green jacket walks through a park and waves","options":{"width":640,"height":360,"fps":12,"duration_seconds":2,"seed":12345,"style":"stylized_3d","action":"walk_and_wave"}}
```

Status request:

```json
{"protocol_version":1,"request_id":"r-3","type":"status","job_id":"job-0123456789abcdef"}
```

Accepted job:

```json
{"protocol_version":1,"request_id":"r-1","ok":true,"job_id":"job-0123456789abcdef","status":"queued"}
```

Completed image:

```json
{"protocol_version":1,"request_id":"r-3","ok":true,"job_id":"job-0123456789abcdef","status":"completed","seed":12345,"result":{"media_type":"image","path":"outputs/job-0123456789abcdef/image.png"},"diagnostics":{"seed":12345}}
```

Error:

```json
{"protocol_version":1,"request_id":"r-1","ok":false,"error":{"code":"UNSUPPORTED_PROMPT_FEATURE","message":"The requested feature 'wings' is not supported yet."}}
```

The client does not choose the output path. The server creates `outputs/<job_id>/image.png` or `outputs/<job_id>/video.mp4` plus `metadata.json`. Job state is also written under `data/jobs`.

Limits:

| Field | Rule |
| --- | --- |
| `protocol_version` | Must be `1` |
| `request_id` | Required, 1–128 characters |
| `prompt` | Required for image and video, 1–1024 characters |
| `width`, `height` | Integers from 16 to 1024 |
| `fps` | Integer from 1 to 30 |
| `duration_seconds` | Integer from 1 to 8 |
| `fps * duration_seconds` | At most 240 frames |
| `seed` | Integer from 0 to 4294967295; generated by the server when omitted |
| `style` | Must be `stylized_3d` |
| `action` | Optional string, at most 64 characters. Recognized values are `walk`, `wave`, `walk_and_wave` |

`options` for an image may only contain `width`, `height`, `seed`, and `style`. A video may also contain `fps`, `duration_seconds`, and `action`. Extra fields are rejected.

Job states: `received`, `validating`, `queued`, `running`, `completed`, `failed`.

Error codes: `MALFORMED_JSON`, `UNSUPPORTED_PROTOCOL_VERSION`, `INVALID_REQUEST_TYPE`, `MISSING_FIELD`, `INVALID_FIELD_VALUE`, `REQUEST_TOO_LARGE`, `UNSUPPORTED_PROMPT_FEATURE`, `UNSUPPORTED_ACTION`, `RESOURCE_LIMIT_EXCEEDED`, `JOB_NOT_FOUND`, `RENDER_FAILED`, `ENCODING_FAILED`, `INTERNAL_ERROR`, `UNAUTHORIZED`.

A completed status response can include `diagnostics.unsupported_terms` for words the parser skipped, plus `frame_count` and `action` for video.

Quick check without the CLI, against a server that is not using TLS:

```bash
printf '%s\n' '{"protocol_version":1,"request_id":"r-1","type":"image","prompt":"A tall woman standing","options":{"width":256,"height":256,"seed":1,"style":"stylized_3d"}}' | nc 127.0.0.1 7070
```

## How a job is built

1. The connection buffer splits the TCP stream into NDJSON lines.
2. The protocol parser checks types, ranges, and which fields belong to that request type.
3. The job manager stores the job and queues it. The socket thread returns `job_id`.
4. The worker parses the prompt into a scene: character, outfit, environment, lighting, pose, and, for video, an action.
5. The character generator builds a stylized mesh (body, hair, clothing) and a pose from the seed and the explicit attributes.
6. The CPU rasterizer draws one frame for an image, or one posed frame per timestamp for a video.
7. PNG bytes are written with a bundled image writer. Video frames are encoded to MP4 when libav is linked.
8. The job record is updated. A later `status` request returns the relative path.

The queue holds 8 jobs by default. Only one job runs at a time. The output directory is limited to 2 GiB by default. Once usage has reached that limit, new jobs are rejected with `RESOURCE_LIMIT_EXCEEDED`. The same code is returned when the queue is full.

## Tests

```bash
ctest --test-dir build --output-on-failure
```

The suite covers framing, protocol validation, the prompt parser, mesh and scene generation, the rasterizer, both pipelines, and deployment behavior (authentication, TLS, the process lock, quotas, and metrics).

## Layout

```text
src/app/         process entry, configuration
src/net/         sockets, optional TLS, NDJSON framing, request dispatch
src/jobs/        queue, worker, image pipeline, video pipeline
src/prompt/      vocabulary, normalizer, prompt parser
src/character/   proportions, palette, skeleton, meshes
src/animation/   walk, wave, and walk-and-wave
src/render/      scene, camera, CPU rasterizer
src/media/       PNG writer and libav encoder
src/storage/     job files, output paths, quota, process lock
src/common/      errors, logging, metrics, token check
tools/cli/       generator_cli
tests/           GoogleTest suite
```

## Security notes

The default bind address is loopback, and authentication is off when you start the server without a config file. Before exposing the port, set `auth_required` and provide a TLS certificate and key. Do not put the token, certificate, or private key in the repository. Request text is not passed to a shell, and output paths are chosen by the server.
