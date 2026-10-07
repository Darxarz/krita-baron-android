# ComfyUI connection for the native Krita port

Acly's Python plugin compiles workflows inside Krita. This temporary adapter moves
that compiler to the ComfyUI machine, retaining the original workflow implementation.
Android Krita contains no Python. The native client sends inference to ComfyUI's
standard `/prompt`, `/ws`, `/history` and `/view` endpoints directly.

1. Install the original plugin's ComfyUI node requirements, including
   [Acly's tooling nodes](https://github.com/Acly/comfyui-tooling-nodes).
2. Copy this directory into ComfyUI's `custom_nodes` directory.
3. Install this repository's `server/requirements.txt` into a separate Python 3.11
   environment on the server. Keep the entire `server/` directory, including `vendor/`.
4. Copy `config.example.json` to `config.json`, and set the absolute Python and
   `compile_workflow.py` paths. These paths cannot be provided by a network request.
5. Restart ComfyUI. Select **Custom Server** in Krita and enter its address.

The adapter prepares workflows and lists models. It never submits, cancels or bills
generation. Access follows the ComfyUI server's existing authentication middleware.
An unmodified stock ComfyUI server does not yet provide these two preparation routes.

Original workflow author: Acly and contributors. Native port and adapter:
Darxarz / Baron Edition. GPL-3.0-or-later.
