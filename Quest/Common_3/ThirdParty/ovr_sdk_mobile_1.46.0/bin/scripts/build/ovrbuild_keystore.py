#!/usr/bin/env python

import os, socket, sys

workingDir = os.getcwd()


def init():
    root = os.path.realpath(os.path.dirname(os.path.realpath(__file__)))
    os.chdir(root)  # make sure we are always executing from the project directory
    while os.path.isdir(os.path.join(root, "bin/scripts/build")) == False:
        root = os.path.realpath(os.path.join(root, ".."))
        if (
            len(root) <= 5
        ):  # Should catch both Posix and Windows root directories (e.g. '/' and 'C:\')
            print("Unable to find SDK root. Exiting.")
            sys.exit(1)
    root = os.path.abspath(root)
    os.environ["OCULUS_SDK_PATH"] = root
    sys.path.append(root + "/bin/scripts/build")


init()
import ovrbuild

ovrbuild.init()


def generate_distinguished_name():
    """
    Generates a moderately unique X.509 distinguished name

    :return: Array of RDN tuples
    """
    user = os.environ.get("USERNAME", os.environ.get("USER", "Unknown"))
    host = socket.gethostname()
    return [("CN", "ovrbuild"), ("CN", user), ("OU", host)]


def encode_distinguished_name(dn):
    """
    Creates a canonical X.509 distinguished name string from an array of RDNs

    :param dn:      Distinguished name stored as array of RDN tuples
    """
    # escape embedded commas in any RDN so that they don't cause problems for
    # keytool's command-line parser
    return ",".join(map(lambda x: x[0] + "=" + x[1].replace(",", r"\,"), dn))


def create_keystore(execfn, path, alias, storepw, keypw, replace=False, sdn=None):
    """
    Creates a JKS keystore with a single private key entry

    :param execfn:  Function to execute shell command, f(List[Tuple]) => Tuple
    :param path:    Path to overwrite with the new keystore
    :param alias:   Alias for the added key entry
    :param storepw: Encryption password for the keystore. Must be >= 6 chars
    :param keypw:   Encryption password for the private key. >= 6 chars.
    :param replace: Overwrite existing keystore file if it exists
    :param sdn:     Array of RDN tuples for the subject distinguished name
    """
    if not (path and alias and storepw and keypw):
        raise ValueError("Missing argument")
    if len(storepw) < 6 or len(keypw) < 6:
        raise ValueError("Insufficient password length")
    if os.path.exists(path) and not replace:
        return None

    if os.path.exists(path):
        os.unlink(path)

    cmd = [
        "keytool",
        "-v",
        "-genkey",
        "-keyalg",
        "RSA",
        "-keystore",
        path,
        "-storepass",
        storepw,
        "-alias",
        alias,
        "-keypass",
        keypw,
        "-validity",
        "10000",
        "-dname",
        encode_distinguished_name(sdn or generate_distinguished_name()),
    ]

    return execfn(cmd)


def genDebugKeystore():
    # verify 'keytool' is on the PATH before trying to execute it.
    if not ovrbuild.check_call(["keytool"]):
        raise EnvironmentError("keytool not found! Verify JDK bin folder is on PATH!")

    os.chdir(workingDir)  # make sure we are always executing from the project directory
    path = os.path.join(".", "android.debug.keystore")
    debug_props = {
        "keystore": path,
        "alias": "androiddebugkey",
        "storepass": "android",
        "keypass": "android",
    }
    create_keystore(
        lambda x: ovrbuild.call(x),
        debug_props["keystore"],
        debug_props["alias"],
        debug_props["storepass"],
        debug_props["keypass"],
        replace=False,
    )


genDebugKeystore()
