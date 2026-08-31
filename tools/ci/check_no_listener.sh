#!/usr/bin/env bash
# Fait échouer le build si du code sous components/ ouvre un listener réseau propre.
# Invariant architectural (docs/SECURITY.md) : bluetooth_proxy n'a aucun code réseau à lui ;
# tout listener parallèle ferait perdre l'authentification Noise héritée d'APIConnection.
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/../.."

PATTERN='AsyncServer|WiFiServer|::listen\s*\(|::bind\s*\(|socket\s*\(\s*AF_INET|lwip_listen|lwip_bind'

if grep -RnE "$PATTERN" components/ --include='*.cpp' --include='*.h'; then
  echo ""
  echo "ERREUR : code ci-dessus semble ouvrir un listener réseau indépendant."
  echo "Invariant violé (docs/SECURITY.md) : aucun composant de ce dépôt ne doit ouvrir de"
  echo "listener réseau propre -- tout doit transiter par les handlers bluetooth_proxy/"
  echo "APIConnection existants pour hériter de l'authentification Noise d'ESPHome."
  exit 1
fi

echo "OK : aucun listener réseau indépendant détecté sous components/."
