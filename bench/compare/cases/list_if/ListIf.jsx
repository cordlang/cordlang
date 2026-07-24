export default function ListDemo({ title = "Items", items = [], selectItem }) {
  const [showHint, setShowHint] = React.useState(1);
  return (
    <div className="flex flex-col gap-4 p-6">
      <h1 className="text-2xl font-bold">{title}</h1>
      {showHint ? <p className="text-gray-500">Hint: click items</p> : null}
      {items.map((item) => (
        <div key={item.id} className="flex gap-2 items-center">
          <span className="font-bold">{item.name}</span>
          <button className="btn outline" onClick={() => selectItem(item.id)}>
            Select
          </button>
        </div>
      ))}
    </div>
  );
}
