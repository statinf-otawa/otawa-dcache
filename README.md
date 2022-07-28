# otawa-icat plugin

This repository implements an [OTAWA](http://otawa.fr) plug-in dedicated to
the analysis of data cache by category. Look to the auto-documentation
for more details.

## User Installation

To install it, either `otawa-config` command must be on the path:

	$ cmake .
	$ make install

Or you can pass otawa-config from a custom location:

	$ cmake . -DOAWA_CONFIG=path_to_otawa_config
	$ make install

In both cases, this plug-in is installed in the plug-in directory
of the `otawa-config` installation.


## Development

### Testing

To enable testing,

	$ cmake . -DWITH_TEST=yes

Currently, testing is only provided for small programs: `singlevar`, `array`, `pointer` or `pointer2`.

The test cases encompass:
  * `access` -- access building
  * `must` -- MUST analysis
  * `pers` -- Persistence analysis
  * `multi` -- Multi persistence analysis
  * `may`-- MAY analysis
  * `event` -- event generation
  * `prefix` -- prefix event generation

To launch a test,

	$ cd test
	$ make test-CASE-PROGRAM
