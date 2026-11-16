# Downloads the official SDL3 development packages for Visual Studio (the "-VC" zips from
# GitHub) and unzips them into deps\SDL3, deps\SDL3_image, deps\SDL3_ttf and
# deps\SDL3_mixer, the folders sdl-game-editor.vcxproj (and the CMake build) look at.
#
# sdl-game-editor.vcxproj runs it by itself on the first build. By hand:
#   powershell -ExecutionPolicy Bypass -File tools\get_sdl.ps1 [-Dest <folder>]
#
# A package that is already there is not downloaded again; delete its folder to get it
# again (for example after changing a version here).

param(
    [string]$Dest = (Join-Path $PSScriptRoot "..\deps")
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"   # the progress bar makes Invoke-WebRequest very slow
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

# Folder name, GitHub repository, version. Keep them in sync with CMakeLists.txt.
$packages = @(
    @{ Name = "SDL3";       Repo = "SDL";       Version = "3.4.14" },
    @{ Name = "SDL3_image"; Repo = "SDL_image"; Version = "3.4.4"  },
    @{ Name = "SDL3_ttf";   Repo = "SDL_ttf";   Version = "3.2.2"  },
    @{ Name = "SDL3_mixer"; Repo = "SDL_mixer"; Version = "3.2.4"  }
)

New-Item -ItemType Directory -Force $Dest | Out-Null
$Dest = (Resolve-Path $Dest).Path

foreach ($p in $packages) {
    $target = Join-Path $Dest $p.Name
    if (Test-Path (Join-Path $target "include")) {
        Write-Host "$($p.Name): already in $target"
        continue
    }

    $zipName = "$($p.Name)-devel-$($p.Version)-VC.zip"
    $url = "https://github.com/libsdl-org/$($p.Repo)/releases/download/release-$($p.Version)/$zipName"
    $temp = Join-Path ([IO.Path]::GetTempPath()) ("sge_" + [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force $temp | Out-Null
    try {
        Write-Host "$($p.Name): downloading $url"
        $zip = Join-Path $temp $zipName
        Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing
        Expand-Archive -Path $zip -DestinationPath $temp -Force
        # The zip holds a single folder named with the version (SDL3-3.4.14): it is moved
        # to a folder without it, so the project paths do not change with the version.
        $inner = Join-Path $temp "$($p.Name)-$($p.Version)"
        if (Test-Path $target) { Remove-Item -Recurse -Force $target }
        Move-Item $inner $target
        Write-Host "$($p.Name): ready in $target"
    }
    catch {
        Write-Host "$($p.Name): could not be downloaded ($($_.Exception.Message))."
        Write-Host "Download $url by hand and unzip it so that $target\include exists."
        exit 1
    }
    finally {
        Remove-Item -Recurse -Force $temp -ErrorAction SilentlyContinue
    }
}
