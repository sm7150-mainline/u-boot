::: sectionauthor
Caleb Connolly \<<caleb.connolly@linaro.org>\>
:::

# Porting U-Boot to Qualcomm SoCs

This document attempts to describe the requirements and steps for supporting
U-Boot on a new Qualcomm SoC. It covers the changes to how Qualcomm support
works in U-Boot and maintainer preferences.

## The old way

Historically, U-Boot was supported on a handful of Qualcomm boards, each device
was a compile time target with its own defconfig, `board/qualcomm/<board>`
directory, sysmap header, memory map, and more. This was how a lot of other
architectures worked, and while not scalable it enabled a given device to be
supported relatively easily albeit with a lot of duplicated code between
targets. Additionally, this support used entirely non-standard devicetree files
which had to be written from scratch for U-Boot for every device.

## The modern way

In 2024, U-Boot is far more adaptable, with many more runtime features and more
generic drivers necessitated by a growing range of supported architectures and
platforms. Modern Qualcomm support in U-Boot leverages this to the greatest
extent possible, as over time improvements to generic code make supporting newer
platforms even easier by providing better abstractions to define the platform
specific changes.

To make this maintainable, we impose the following requirements:

### All boards must use an upstream-compliant devicetree

Where possible, DT should be imported from Linux (or used directly from the
devicetree-rebasing subtree once it's available). If the board doesn't have an
upstream DT then the DT used for U-Boot should be the same as the one being used
to boot Linux. Any U-Boot specific DT modifications must be done in a
`<board>-u-boot.dtsi` file with a comment justifying each change.

### Board specific features must be selected at runtime where possible

If a given board requires a workaround that doesn't belong in any given driver,
it should be added to `arch/arm/mach-snapdragon/board.c` and should implement
runtime detection to determine if it's necessary. This detection should be based
on the relevant *feature* of the platform rather than simply by matching on the
platform itself. This will ensure that if other platforms need the wrokaround
that it will be detected as appropriate.

For example, the db410c only has a single USB controller, but is capable of
operating in host or peripheral mode. When switching modes it must adjust some
GPIOs to enable the built in USB hub and configure the mux. The pin
configuration is defined in the USB node in DT as a pinctrl named "device", the
previous solution hardcoded the GPIO numbers in the db410c board code and set
the values as needed. The new approach instead checks to see if the USB device
has a pinctrl setting named "device" (which is a non-standard name) and if so
then selects the relevant pinctrl state to switch modes.

### Boards that can boot with the generic qcom_defconfig, should

If your board doesn't require any special non-standard behaviour or features
which can't be elagently handled at runtime and break compatibility with
existing boards, then it should be supported by the generic `qcom_defconfig`,
even if this only provides a subset of all functionality or is incompatible with
your production usecase. This simplifies U-Boot build testing (ensuring it
covers your SoC specific drivers) and provides a good starting point for any
community efforts around that SoC or board.

## So how do I add a new SoC?

Usually, the minimum number of a changes for a new SoC is a stub implementation
of the clock and pinctrl drivers. This don't need to do anything except probe
and return the expected values in their callbacks. This should be enough to get
to a U-Boot shell.

Depending on the board, the relevant compatible should be added to the
`qcom-hyp-smmu` driver.

## New board, existing SoC

Just import your DT from Linux and go! If working on a phone, make sure you add
a framebuffer node if UART isn't available. If you're embedding the DT instead
of chainloading from ABL then you also need to add a `<board>-u-boot.dtsi` file
with a populated `/memory` node. See `sdm845-db845c-u-boot.dtsi` for an example.
