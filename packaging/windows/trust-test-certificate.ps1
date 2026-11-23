# SPDX-License-Identifier: GPL-2.0-or-later
#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Trusts (or stops trusting) the self-signed certificate used to sign test builds.

.DESCRIPTION
    Test builds are signed with a self-signed "Aragami Korosu" certificate. Windows only shows
    that name as the publisher on machines that trust it. This adds the public certificate
    to the local machine's Trusted Root and Trusted Publishers stores, which the
    administrator (UAC) prompt also uses.

    Only trust certificates you created yourself. Run with -Remove to undo.

.EXAMPLE
    .\trust-test-certificate.ps1
.EXAMPLE
    .\trust-test-certificate.ps1 -Remove
#>
param(
	[string] $Certificate = (Join-Path $PSScriptRoot 'aragami-korosu-signing.cer'),
	[switch] $Remove
)

$ErrorActionPreference = 'Stop'
$cert = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2 $Certificate
Write-Host "Certificate: $($cert.Subject)  (thumbprint $($cert.Thumbprint), expires $($cert.NotAfter.ToString('yyyy-MM-dd')))"

foreach ($storeName in 'Root', 'TrustedPublisher') {
	$store = New-Object System.Security.Cryptography.X509Certificates.X509Store $storeName, 'LocalMachine'
	$store.Open('ReadWrite')
	if ($Remove) {
		$store.Remove($cert)
		Write-Host "Removed from LocalMachine\$storeName"
	} else {
		$store.Add($cert)
		Write-Host "Added to LocalMachine\$storeName"
	}
	$store.Close()
}
