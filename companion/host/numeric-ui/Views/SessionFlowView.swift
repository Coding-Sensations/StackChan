import SwiftUI
struct SessionFlowView: View {
    let stage: CompanionStage
    let readyNow: Bool
    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text(readyNow ? "Ready now to pair" : stage.title).font(.headline)
            HStack(spacing: 12) {
                ForEach(CompanionStage.allCases, id: \.self) { item in
                    Image(systemName: item == stage ? "largecircle.fill.circle" : "circle")
                        .foregroundStyle(item == stage ? Color.accentColor : Color.secondary)
                        .help(item.title).accessibilityLabel(item.title + (item == stage ? ", current stage" : ""))
                }
            }
        }.accessibilityElement(children: .combine)
    }
}
