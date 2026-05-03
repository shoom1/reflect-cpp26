# Contributing

## Bugs

Open an issue with: compiler + version, flags, minimal reproducer, expected vs actual.

## Pull requests

- One change per PR; branch off `main`.
- Add a test in `tests/` for any behavior change.
- `./scripts/docker-test.sh` must be green before review.
- Update `README.md` if you touch the public API.

## Style

C++26, header-only, no third-party deps. Public API lives in `namespace reflect`;
implementation in nested `*_detail` namespaces.

## License

Contributions are licensed under MIT (see `LICENSE`).
