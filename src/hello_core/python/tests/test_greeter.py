from hello_core import Greeter, HelloCoreError


def test_greet_basic() -> None:
    with Greeter("World") as greeter:
        assert greeter.greet() == "Hello, World!"


def test_greeter_reusable_after_close() -> None:
    greeter = Greeter("Ada")
    assert greeter.greet() == "Hello, Ada!"
    greeter.close()
    greeter.close()  # double-close must be a no-op, not a crash


def test_error_type_importable() -> None:
    assert issubclass(HelloCoreError, RuntimeError)
