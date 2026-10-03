# Retention probe h_retention_restart_20261002_161234_6916

## Scope
Isolated HarnessKit graph snapshot; owner exit and daemon restart.

## Artifact
Artifact ID: ART-RETENTION-202610021612346916
Expected SHA-256: d1b75ba4a8468e6a58149c1c51d032145aa05b0041e9b0ca4b7089e72b7b7b24
Exact text: retention_probe|owner_exit|daemon_restart|h_retention_restart_20261002_161234_6916

## Graph
```tactical-spec
{
  "nodes": [
    {
      "symbol": "AR-01",
      "type": "FractalArtifact",
      "cbm_uri": "cbm://C-Users-corre-Documents-harness-kit/docs/temenos/h_retention_restart_20261002_161234_6916/retention-probe.md#AR-01",
      "description": "{\"id\":\"AR-01\",\"artifact_id\":\"ART-RETENTION-202610021612346916\",\"temenos_id\":\"h_retention_restart_20261002_161234_6916\",\"part_count\":2,\"part_ids\":[\"ART-RETENTION-202610021612346916-P01\",\"ART-RETENTION-202610021612346916-P02\"],\"join_policy\":\"direct\",\"byte_count\":82,\"sha256\":\"d1b75ba4a8468e6a58149c1c51d032145aa05b0041e9b0ca4b7089e72b7b7b24\"}"
    },
    {
      "symbol": "AP-01",
      "type": "FractalArtifactPart",
      "cbm_uri": "cbm://C-Users-corre-Documents-harness-kit/docs/temenos/h_retention_restart_20261002_161234_6916/retention-probe.md#AP-01",
      "description": "{\"id\":\"AP-01\",\"artifact_id\":\"ART-RETENTION-202610021612346916\",\"temenos_id\":\"h_retention_restart_20261002_161234_6916\",\"part_id\":\"ART-RETENTION-202610021612346916-P01\",\"part_index\":1,\"total_parts\":2,\"exact_content\":\"retention_probe|owner_exit|daemon_restart\",\"byte_count\":41,\"sha256\":\"5cf584fc7934436d0c2ac78e7019b13e21a7324bf9e5d42231bbb84ce6d81d0b\"}"
    },
    {
      "symbol": "AP-02",
      "type": "FractalArtifactPart",
      "cbm_uri": "cbm://C-Users-corre-Documents-harness-kit/docs/temenos/h_retention_restart_20261002_161234_6916/retention-probe.md#AP-02",
      "description": "{\"id\":\"AP-02\",\"artifact_id\":\"ART-RETENTION-202610021612346916\",\"temenos_id\":\"h_retention_restart_20261002_161234_6916\",\"part_id\":\"ART-RETENTION-202610021612346916-P02\",\"part_index\":2,\"total_parts\":2,\"exact_content\":\"|h_retention_restart_20261002_161234_6916\",\"byte_count\":41,\"sha256\":\"34a29845bd9024a9cdc4c82b57956c812ecbf4c30ad198c6eacac7442b26b7c9\"}"
    }
  ],
  "edges": [
    {
      "source": "AR-01",
      "target": "AP-01",
      "type": "CONTAINS_PART"
    },
    {
      "source": "AP-01",
      "target": "AP-02",
      "type": "NEXT_PART"
    },
    {
      "source": "AR-01",
      "target": "AP-02",
      "type": "CONTAINS_PART"
    }
  ]
}
```
