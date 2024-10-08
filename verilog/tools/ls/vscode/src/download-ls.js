"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.checkAndDownloadBinaries = void 0;
const os_1 = require("os");
const fs = require("fs");
const path = require("path");
const follow_redirects_1 = require("follow-redirects");
const child_process_1 = require("child_process");
const decompress = require("decompress");
const decompressTargz = require("decompress-targz");
const decompressUnzip = require("decompress-unzip");
const TAG = require("../package.json").repository.tag;
function checkIfBinaryExists(binaryPath) {
    let whichCommand;
    let binaryExists;
    if ((0, os_1.platform)() == "win32") {
        let parsedBinPath = path.parse(binaryPath);
        whichCommand = `where "${parsedBinPath.dir}:${parsedBinPath.base}"`;
    }
    else {
        whichCommand = `command -v ${binaryPath}`;
    }
    binaryExists = true;
    try {
        (0, child_process_1.execSync)(whichCommand, { windowsHide: true });
    }
    catch {
        binaryExists = false;
    }
    return binaryExists;
}
async function checkAndDownloadBinaries(binaryPath, output) {
    output.appendLine("Platform: '" + (0, os_1.platform)() + "'");
    // Update home paths to an absolute path
    if ((0, os_1.platform)() != "win32" && binaryPath.startsWith("~/")) {
        binaryPath = binaryPath.replace("~", "");
        binaryPath = path.join((0, os_1.homedir)(), binaryPath);
        output.appendLine(`Adjusted server path: ${binaryPath}`);
    }
    if (checkIfBinaryExists(binaryPath)) {
        // Language server binary exists -- nothing to do
        return binaryPath;
    }
    output.appendLine(`Set language server executable (${binaryPath}) doesn't exist or cannot be accessed`);
    if ((0, os_1.platform)() === "darwin") {
        // There is no static binaries for MacOS -- aborting
        output.appendLine("GitHub release is not available for MacOS. Language server can be installed with:");
        output.appendLine("\tbrew tap chipsalliance/verible");
        output.appendLine("\tbrew install verible");
        return binaryPath;
    }
    const pluginDir = path.join(__dirname, "..");
    const binDir = path.join(pluginDir, "bin");
    const pluginBinaryPath = path.join(binDir, "verible-verilog-ls" + ((0, os_1.platform)() === "win32" ? ".exe" : ""));
    if (checkIfBinaryExists(pluginBinaryPath)) {
        output.appendLine("Language server binary already downloaded");
        return pluginBinaryPath;
    }
    // Retreving tag based on plugin's version
    if (TAG === undefined || TAG === null) {
        // No tag found -- aborting
        return binaryPath;
    }
    output.appendLine(`Extension will attempt to download executables (${TAG}) from GitHub Release page`);
    // Preparing URL
    let platformName, extension, archName = "";
    if ((0, os_1.platform)() === "win32") {
        platformName = "win64";
        extension = "zip";
    }
    else {
        platformName = "linux-static";
        archName = (0, os_1.arch)() === "x64" ? "-x86_64" : "-aarch64";
        extension = "tar.gz";
    }
    const releaseUrl = `https://github.com/chipsalliance/verible/releases/download/${TAG}/verible-${TAG}-${platformName}${archName}.${extension}`;
    // Creating bin directory
    fs.mkdirSync(binDir, { recursive: true });
    // Downloading release
    const archivePath = path.join(pluginDir, `verible.${extension}`);
    const archive = fs.createWriteStream(archivePath);
    await new Promise((resolve, reject) => follow_redirects_1.https.get(releaseUrl, (response) => {
        if (response.statusCode !== 200) {
            output.appendLine("Download failed with status code " + response.statusCode);
            reject("Status code " + response.statusCode);
        }
        response.pipe(archive);
        archive.on("finish", () => {
            archive.close();
            resolve();
        });
    }).on("error", (_err) => {
        output.appendLine("Failed to start download");
        return binaryPath;
    }));
    // Unpacking and removing downloaded archive
    await decompress(archivePath, binDir, {
        filter: (file) => path.basename(file.path).startsWith("verible-verilog-ls"),
        map: (file) => {
            file.path = path.basename(file.path);
            return file;
        },
        plugins: (0, os_1.platform)() === "win32" ? [decompressUnzip()] : [decompressTargz()],
    }).catch((_err) => {
        return binaryPath;
    })
        .finally(() => {
        fs.rm(archivePath, () => null);
    });
    return pluginBinaryPath;
}
exports.checkAndDownloadBinaries = checkAndDownloadBinaries;
//# sourceMappingURL=download-ls.js.map